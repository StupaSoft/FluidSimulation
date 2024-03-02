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
layout(location = 0) in vec3 inPosition; // Particle center position (not this vertex position)
layout(location = 1) in vec3 inNormal; // In this shader, this is used as [radius, 0.0f, 0.0f]
layout(location = 2) in vec2 inTexCoord; // In this shader, this is used as a position offset

// Output
layout(location = 0) out vec3 particlePos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragTexCoord;

layout(location = 3) out vec3 specularColor;
layout(location = 4) out float glossiness;

layout(location = 5) out vec3 lightDirection;
layout(location = 6) out vec3 lightColor;
layout(location = 7) out float lightIntensity;

layout(location = 8) out vec3 viewDirection;

void main()
{
    // Billboarding
    float radius = inNormal.x;

    vec3 worldVertexPos = inPosition;
    particlePos = (mvp.proj * mvp.view * vec4(worldVertexPos, 1.0f)).xyz;

    vec2 offset = inTexCoord * radius; // (camera right, camera up) component

    vec3 cameraRight = vec3(mvp.view[0][0], mvp.view[1][0], mvp.view[2][0]);
    vec3 cameraUp = vec3(mvp.view[0][1], mvp.view[1][1], mvp.view[2][1]);
    worldVertexPos += (offset.x * cameraRight + offset.y * cameraUp);

    gl_Position = mvp.proj * mvp.view * vec4(worldVertexPos, 1.0f);

    // Others
    mat4 modelView = mvp.view * mvp.model;
    vec4 viewPosition = modelView * vec4(inPosition, 1.0f);

    fragColor = material.color.xyz;
    fragTexCoord = inTexCoord;

    specularColor = material.specularColor.xyz;
    glossiness = material.glossiness;

    lightDirection = normalize((mvp.view * light.direction).xyz);
    lightColor = light.color.xyz;
    lightIntensity = light.intensity;

    viewDirection = -viewPosition.xyz;
}