#include "Vertex.h"

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = // At which rate to load data from memory throughout the vertices
	{
		.binding = 0, // The index of the binding in the array of bindings
		.stride = sizeof(Vertex), // The number of bytes from one entry to the next
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX // Move to the next data entry after each vertex
	};

	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
{
	// How to extract a vertex attribute from a chunk of vertex data originating from a binding description
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions =
	{
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, pos)
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color)
		},
		{
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(Vertex, texCoord)
		}
	};

	return attributeDescriptions;
}