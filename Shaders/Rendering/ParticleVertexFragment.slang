import RenderModule;

struct VertexToPixel
{
    float4 position : SV_Position; // Position of this vertex
    float3 particlePosition; // Particle center position (not this vertex position)

    float3 color;
    float3 normal; // View space
    float2 texCoord;

    float3 specularColor;
    float glossiness;

    float3 lightDirection; // View space
    float3 lightColor;
    float lightIntensity;

    float3 viewDirection;
}

ConstantBuffer<MVP> mvp;
ConstantBuffer<Light> light;
ConstantBuffer<Material> material;
uniform Sampler2D texSampler;

[shader("vertex")]
VertexToPixel VSMain(VertexInput input)
{
    // Billboarding
    VertexToPixel output;

    float radius = input.normal.x; // In this shader, normal is used as [particle radius, 0.0f, 0.0f]

    float3 worldVertexPos = input.position; // input.position is the particle center's position (not this vertex's position)
    output.particlePosition = mul(mul(mvp.proj, mvp.view), float4(worldVertexPos, 1.0f)).xyz;

    // In this shader, input.texCoord is used as a position offset
    float2 offset = input.texCoord * radius; // (camera right, camera up) component

    float3 cameraRight = float3(mvp.view[0][0], mvp.view[0][1], mvp.view[0][2]);
    float3 cameraUp = float3(mvp.view[1][0], mvp.view[1][1], mvp.view[1][2]);
    worldVertexPos += (offset.x * cameraRight + offset.y * cameraUp);

    output.position = mul(mul(mvp.proj, mvp.view), float4(worldVertexPos, 1.0f));

    // Others
    float4x4 modelView = mul(mvp.view, mvp.model);
    float4 viewPosition = mul(modelView, float4(input.position, 1.0f)); 

    output.color = material.materialColor.xyz;
    output.normal = mul(modelView, float4(input.normal, 0.0f)).xyz; // This is a normal transformation (see normal matrix)
    output.texCoord = input.texCoord;

    output.specularColor = material.materialSpecularColor.xyz;
    output.glossiness = material.materialGlossiness;

    output.lightDirection = normalize(mul(mvp.view, light.lightDirection).xyz);
    output.lightColor = light.lightColor.xyz;
    output.lightIntensity = light.lightIntensity;

    output.viewDirection = -viewPosition.xyz;

    return output;
}

[shader("fragment")]
float4 PSMain(VertexToPixel input)
{
    float distanceFromCenter = input.texCoord.x * input.texCoord.x + input.texCoord.y * input.texCoord.y;

    if (distanceFromCenter > 1.0f) discard; // Trim the quad to be a spherical shape

    float coordZ = sqrt(1.0f - distanceFromCenter);
    float3 fragNormal = float3(input.texCoord, coordZ); // Spherical normal

    // Diffuse
    float3 diffuse = input.lightIntensity * (input.lightColor * clamp(dot(fragNormal, input.lightDirection), 0.0f, 1.0f)) * input.color;

    // Specular
    float3 halfway = normalize(normalize(input.viewDirection) + normalize(input.lightDirection));
    float highlight = pow(clamp(dot(fragNormal, halfway), 0.0f, 1.0f), input.glossiness) * float(dot(fragNormal, input.lightDirection) > 0.0f);
    float3 specular = (input.lightColor * input.specularColor) * highlight;

    return float4(diffuse + specular, 1.0f);
}

