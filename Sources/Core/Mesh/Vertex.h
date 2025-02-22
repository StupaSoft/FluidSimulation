#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex
{
	glm::vec3 pos{};
	glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec2 texCoord{};

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};