import RenderModule;

struct Sphere
{
    float4 pos;
    float radius;
};

ConstantBuffer<float2> screenSize;
ConstantBuffer<float3> cameraPos;
ConstantBuffer<Light> light;

ConstantBuffer<Sphere> targetSphere;
ConstantBuffer<Sphere> learningSphere;

ConstantBuffer<Material> targetMaterial;
ConstantBuffer<Material> learningMaterial;

RWStructuredBuffer<uint8_t4_packed> targetScene;
RWStructuredBuffer<uint8_t4_packed> renderedScene;

// Return the travel distance to the nearest 
float RaySphereIntersect(float3 rayOrigin, float3 rayDirection, float3 center, float radius)
{
    float3 L = center - rayOrigin;
    float tca = dot(L, rayDirection);

    if (tca < 0.0f) 
        return -1.0f;

    float d2 = length(L) * length(L) - tca * tca;
    float r2 = radius * radius;

    if (d2 > r2)
        return -1.0f;

    float thc = sqrt(r2 - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;

    if (t0 >= 0.0f) 
        return t0;

    if (t1 >= 0.0f) 
        return t1;

    return -1.0f;
}

[Differentiable]
float4 Loss(float4 previous, float3 color, float3 normal, float3 specularColor, float glossiness, float3 lightDirection, float3 lightColor, float3 lightIntensity, float3 viewDirection)
{
    return BlinnPhong(color, normal, specularColor, glossiness, lightDirection, lightColor, lightIntensity, viewDirection);
}

[Differentiable]
float4 BlinnPhong(float3 color, float3 normal, float3 specularColor, float glossiness, float3 lightDirection, float3 lightColor, float3 lightIntensity, float3 viewDirection)
{
    float3 normal = normalize(normal);
    float3 diffuse = (lightColor * dot(normal, lightDirection) * lightIntensity) * color;

    float3 halfway = normalize(normalize(viewDirection) + normalize(lightDirection));
    float highlight = pow(dot(normal, halfway), glossiness) * dot(normal, lightDirection);
    float3 specular = (lightColor * specularColor) * highlight;

    return float4(diffuse + specular, 1.0f); // View space
}

[shader("compute")]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    uint x = globalThreadID.x;
    uint y = globalThreadID.y;

    // Calculate the ray
    float3 rayOrigin = cameraPos;
    float3 screenPoint = float3(-screenSize / 2.0f + float2(x, y), 0.0f) / 1000.0f;
    float3 rayDirection = normalize(screenPoint - rayOrigin);

    // 1. Raytrace the target sphere
    float targetSphereHit = RaySphereIntersect(rayOrigin, rayDirection, targetSphere.pos.xyz, targetSphere.radius);
    if (targetSphereHit != -1.0f)
    {
        uint4 color = uint4(255, 0, 0, 255);
        renderedScene[y * 1920 + x] = pack_u8(color);
    }

    // 2. Raytrace the learning sphere
    float learningSphereHit = RaySphereIntersect(rayOrigin, rayDirection, learningSphere.pos.xyz, learningSphere.radius);
    if (learningSphereHit != -1.0f)
    {
        uint4 color = uint4(0, 0, 255, 255);
        renderedScene[y * 1920 + x] = pack_u8(color);
    }
}

