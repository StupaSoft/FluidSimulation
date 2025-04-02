import RenderModule;

struct Sphere
{
    float4 pos;
    float radius;
};

ConstantBuffer<uint2> screenSize;
ConstantBuffer<float3> cameraPos;
ConstantBuffer<Light> light;

ConstantBuffer<Sphere> targetSphere;
ConstantBuffer<Sphere> learningSphere;

ConstantBuffer<Material> targetMaterial;
RWStructuredBuffer<Material> learningMaterial;
RWStructuredBuffer<Material> gradientMaterial; // Gradient

RWStructuredBuffer<float4> targetScene;
RWStructuredBuffer<float4> renderedScene;

// Return the travel distance to the nearest 
float RaySphereIntersect(float4 rayOrigin, float4 rayDirection, float4 center, float radius)
{
    float4 L = center - rayOrigin;
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

// Calculate a distance between a target pixel and a pixel rendered by the current material
[Differentiable]
float Loss(float4 target, float4 normal, Material mat, Light light, float4 viewDirection)
{
    return pow(length(target - BlinnPhong(normal, mat, light, viewDirection)), 2.0f);
}

[Differentiable]
float4 BlinnPhong(float4 normal, Material mat, Light light, float4 viewDirection)
{
    normal = normalize(normal);
    float4 lightDirection = -light.direction;
    float4 diffuse = light.color * light.intensity * dot(normal, lightDirection) * mat.color;

    float4 halfway = normalize(normalize(viewDirection) + lightDirection);
    float highlight = pow(dot(normal, halfway), mat.glossiness) * dot(normal, lightDirection);
    float4 specular = light.color * mat.specularColor * highlight;

    return diffuse + specular; // View space
}

[shader("compute")]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    uint x = globalThreadID.x;
    uint y = globalThreadID.y;
    uint entry = y * screenSize.x + x;

    // Calculate the ray
    float4 rayOrigin = float4(cameraPos, 0.0f);
    float4 screenPoint = float4(-(float2)screenSize / 2.0f + float2(x, y), 0.0f, 0.0f) / 1000.0f;
    float4 rayDirection = normalize(screenPoint - rayOrigin);

    // 1. Raytrace the target sphere to draw and store the result in the learning target buffer
    float targetSphereHit = RaySphereIntersect(rayOrigin, rayDirection, targetSphere.pos, targetSphere.radius);
    if (targetSphereHit != -1.0f)
    {
        float4 hitPoint = rayOrigin + rayDirection * targetSphereHit;
        float4 normal = hitPoint - targetSphere.pos;
        float4 viewDirection = rayOrigin - hitPoint;
        float4 pixel = BlinnPhong(normal, targetMaterial, light, viewDirection);
        renderedScene[entry] = pixel;
        targetScene[entry] = pixel;
    }

    // 2. Raytrace the learning sphere just to draw
    float learningSphereHit = RaySphereIntersect(rayOrigin, rayDirection, learningSphere.pos, learningSphere.radius);
    if (learningSphereHit != -1.0f)
    {
        float4 hitPoint = rayOrigin + rayDirection * learningSphereHit;
        float4 normal = hitPoint - learningSphere.pos;
        float4 viewDirection = rayOrigin - hitPoint;
        float4 pixel = BlinnPhong(normal, learningMaterial[0], light, viewDirection);
        renderedScene[entry] = pixel;
    }

    // 3. Calculate gradient
    // To compute a loss, we render a scene using the geometry of the target sphere and the learning material
    // and then compare it with the target scene
    float diffSphereHit = RaySphereIntersect(rayOrigin, rayDirection, targetSphere.pos, targetSphere.radius);
    if (diffSphereHit != -1.0f)
    {
        float4 hitPoint = rayOrigin + rayDirection * diffSphereHit;
        float4 normal = hitPoint - targetSphere.pos;
        float4 viewDirection = rayOrigin - hitPoint;

        DifferentialPair<float4> targetPair = diffPair(targetScene[entry]);
        DifferentialPair<Material> matPair = diffPair(learningMaterial[0]);
        DifferentialPair<Light> lightPair = diffPair((Light)light);
        DifferentialPair<float4> normalPair = diffPair(normal);
        DifferentialPair<float4> viewDirectionPair = diffPair(viewDirection);

        bwd_diff(Loss)(targetPair, normalPair, matPair, lightPair, viewDirectionPair, 1.0f);
        if (!isnan(matPair.d.color.x) && !isnan(matPair.d.color.y) && !isnan(matPair.d.color.z))
        {
            // Aggregate color gradients
            InterlockedAdd(gradientMaterial[0].color.x, matPair.d.color.x);
            InterlockedAdd(gradientMaterial[0].color.y, matPair.d.color.y);
            InterlockedAdd(gradientMaterial[0].color.z, matPair.d.color.z);
        }
        
        if (!isnan(matPair.d.specularColor.x) && !isnan(matPair.d.specularColor.y) && !isnan(matPair.d.specularColor.z))
        {
            // Aggregate specular color gradients
            InterlockedAdd(gradientMaterial[0].specularColor.x, matPair.d.specularColor.x);
            InterlockedAdd(gradientMaterial[0].specularColor.y, matPair.d.specularColor.y);
            InterlockedAdd(gradientMaterial[0].specularColor.z, matPair.d.specularColor.z);
        }

        if (!isnan(matPair.d.glossiness))
        {
            // Aggregate glossiness
            InterlockedAdd(gradientMaterial[0].glossiness, matPair.d.glossiness);
        }
    }
}

