import RenderModule;

struct VertexToPixel
{
    float4 position : SV_Position;

    float3 color;
    float3 normal; // View space
    float2 texCoord;

    float3 specularColor;
    float glossiness;

    float3 direction; // View space
    float3 lightColor;
    float intensity;
    
    float3 viewDirection;
}

ConstantBuffer<MVP> mvp;
ConstantBuffer<Light> light;
ConstantBuffer<Material> material;
Sampler2D texSampler;

[shader("vertex")]
VertexToPixel VSMain(VertexInput input)
{
    float4x4 modelView = mul(mvp.view, mvp.model);

    float4 viewPosition = mul(modelView, float4(input.position, 1.0f));

    VertexToPixel output;
    output.position = mul(mvp.proj, viewPosition);

    output.color = material.color.xyz;
    output.normal = mul(modelView, float4(input.normal, 0.0f)).xyz; // This is a normal transformation (see normal matrix)
    output.texCoord = input.texCoord;

    output.specularColor = material.specularColor.xyz;
    output.glossiness = material.glossiness;

    output.direction = normalize(mul(mvp.view, light.direction).xyz);
    output.lightColor = light.color.xyz;
    output.intensity = light.intensity;

    output.viewDirection = -viewPosition.xyz;

    return output;
}

[shader("fragment")]
float4 PSMain(VertexToPixel input)
{
    float3 normal = normalize(input.normal);
    float3 diffuse = (((input.color * clamp(dot(normal, input.direction), 0.0f, 1.0f)) * input.intensity) * texSampler.Sample(input.texCoord).xyz) * input.lightColor;

    float3 halfway = normalize(normalize(input.viewDirection) + normalize(input.direction));
    float highlight = pow(clamp(dot(normal, halfway), 0.0f, 1.0f), input.glossiness) * float(dot(normal, input.direction) > 0.0f);
    float3 specular = (input.color * input.specularColor) * highlight;

    return float4(diffuse + specular, 1.0f); // View space
}

[shader("fragment")]
float4 PSFlatMain(VertexToPixel input)
{
    float3 diffuse = texSampler.Sample(input.texCoord).xyz;
    return float4(diffuse, 1.0f); // View space
}


