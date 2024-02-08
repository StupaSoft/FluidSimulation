#pragma once

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Triangle
{
	glm::vec3 A{};
	glm::vec3 B{};
	glm::vec3 C{};

	glm::vec3 normalA{};
	glm::vec3 normalB{};
	glm::vec3 normalC{};
};

