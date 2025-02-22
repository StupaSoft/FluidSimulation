#pragma once

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

struct Triangle
{
	alignas(16) glm::vec4 A{};
	alignas(16) glm::vec4 B{};
	alignas(16) glm::vec4 C{};

	alignas(16) glm::vec4 normalA{};
	alignas(16) glm::vec4 normalB{};
	alignas(16) glm::vec4 normalC{};
};

