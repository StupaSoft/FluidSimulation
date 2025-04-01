#pragma once

#include "glm/glm.hpp"

struct Light
{
	alignas(16) glm::vec4 _direction; // Use vec4 instead of vec3 so that it can match the shader alignment
	alignas(16) glm::vec4 _color; // Ditto
	alignas(4) float _intensity;
};

struct Material
{
	alignas(16) glm::vec4 _color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	alignas(16) glm::vec4 _specularColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);
	alignas(4) float _glossiness = 10.0f;
};