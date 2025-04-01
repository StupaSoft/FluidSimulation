import RenderModule;

struct VertexToPixel
{
    float4 position : SV_Position;

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
Sampler2D texSampler;

[shader("vertex")]
VertexToPixel VSMain(VertexInput input)
{
    float4x4 modelView = mul(mvp.view, mvp.model);

    float4 viewPosition = mul(modelView, float4(input.position, 1.0f));

    VertexToPixel output;
    output.position = mul(mvp.proj, viewPosition);

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
    float3 normal = normalize(input.normal);
    float3 diffuse = (((input.lightColor * clamp(dot(normal, input.lightDirection), 0.0f, 1.0f)) * input.lightIntensity) * texSampler.Sample(input.texCoord).xyz) * input.color;

    float3 halfway = normalize(normalize(input.viewDirection) + normalize(input.lightDirection));
    float highlight = pow(clamp(dot(normal, halfway), 0.0f, 1.0f), input.glossiness) * float(dot(normal, input.lightDirection) > 0.0f);
    float3 specular = (input.lightColor * input.specularColor) * highlight;

    return float4(diffuse + specular, 1.0f); // View space
}

[shader("fragment")]
float4 PSFlatMain(VertexToPixel input)
{
    float3 diffuse = texSampler.Sample(input.texCoord).xyz;
    return float4(diffuse, 1.0f); // View space
}


