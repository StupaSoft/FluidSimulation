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

// Input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// Output
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(location = 2) out vec3 lightDirection;
layout(location = 3) out vec3 lightColor;
layout(location = 4) out float lightIntensity;

void main()
{
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPosition, 1.0);

    fragColor = inColor + lightColor;
    fragTexCoord = inTexCoord;

    lightDirection = light.direction.xyz;
    lightColor = light.color.xyz;
    lightIntensity = light.intensity;
}