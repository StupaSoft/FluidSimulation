#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

inline bool operator==(const Vertex &lhs, const Vertex &rhs)
{
	return lhs.color == rhs.color && lhs.pos == rhs.pos && lhs.texCoord == rhs.texCoord;
}

template<> 
struct std::hash<Vertex>
{
	size_t operator()(const Vertex &vertex) const
	{
		return hash<glm::vec3>()(vertex.pos) ^ ((hash<glm::vec3>()(vertex.color) << 1) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};
