#version 450

// Uniform
layout(binding = 3) uniform sampler2D texSampler;

// Input
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal; // View space
layout(location = 2) in vec2 fragTexCoord;

layout(location = 3) in vec3 specularColor;
layout(location = 4) in float glossiness;

layout(location = 5) in vec3 lightDirection; // View space
layout(location = 6) in vec3 lightColor;
layout(location = 7) in float lightIntensity;

layout(location = 8) in vec3 viewDirection;

// Output
layout(location = 0) out vec4 outColor; // View space

void main()
{
    // Diffuse
    vec3 normal = normalize(fragNormal);
    vec3 diffuse = lightIntensity * (lightColor * clamp(dot(normal, lightDirection), 0.0f, 1.0f)) * texture(texSampler, fragTexCoord).xyz * fragColor;

    // Specular
    vec3 halfway = normalize(normalize(viewDirection) + normalize(lightDirection));
    float highlight = pow(clamp(dot(normal, halfway), 0.0f, 1.0f), glossiness) * float(dot(normal, lightDirection) > 0.0f);
    vec3 specular = lightColor * specularColor * highlight;

    outColor = vec4(diffuse + specular, 1.0f);
}