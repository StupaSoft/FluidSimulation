#version 450

// Uniform
layout(binding = 0) uniform MVP
{
    mat4 model;
    mat4 view;
    mat4 proj;
} 
mvp;

layout(binding = 1) uniform Light
{
    vec4 direction;
    vec4 color;
    float intensity;
}
light;

layout(binding = 2) uniform Material
{
    vec4 color;
    vec4 specularColor;
    float glossiness;
}
material;

// Input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Output
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

layout(location = 3) out vec3 specularColor;
layout(location = 4) out float glossiness;

layout(location = 5) out vec3 lightDirection;
layout(location = 6) out vec3 lightColor;
layout(location = 7) out float lightIntensity;

layout(location = 8) out vec3 viewDirection;

void main()
{
    mat4 modelView = mvp.view * mvp.model;
    mat4 inverseModelView = transpose(modelView);

    vec4 viewPosition = modelView * vec4(inPosition, 1.0f);
    gl_Position = mvp.proj * viewPosition;

    fragColor = material.color.xyz;
    fragNormal = (modelView * vec4(inNormal, 0.0f)).xyz; // Actually, this is a normal transformation (see normal matrix)
    fragTexCoord = inTexCoord;

    specularColor = material.specularColor.xyz;
    glossiness = material.glossiness;

    lightDirection = normalize((mvp.view * light.direction).xyz);
    lightColor = light.color.xyz;
    lightIntensity = light.intensity;

    viewDirection = -viewPosition.xyz;
}