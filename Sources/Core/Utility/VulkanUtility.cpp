#include "VulkanUtility.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "VulkanCore.h"

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &requiredProperties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties); // Query info about the available types of memory

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties) // Check if the memory satisfies the condition
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find a suitable memory type.");
}

VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice) // Return min(depth sample count, color sample count)
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags sampleCountFlag = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	std::vector<VkSampleCountFlagBits> countFlags = { VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_1_BIT };
	for (const auto flag : countFlags)
	{
		if (sampleCountFlag & flag) return flag;
	}

	return VK_SAMPLE_COUNT_1_BIT;
}

std::vector<char> ReadFile(const std::string &filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error(std::format("Failed to open the file {0}", filePath));
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::string &OBJFileName)
{
	std::string OBJPath = MODEL_DIR + OBJFileName;
	
	// Mimick contents of tinyobj::LoadObj to support Unicode for file names
	tinyobj::attrib_t attribute; // Holds all of the positions, normals, and texture coordinates
	std::vector<tinyobj::shape_t> shapes; // Contains all of the separate objects and their faces
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	attribute.vertices.clear();
	attribute.normals.clear();
	attribute.texcoords.clear();
	attribute.colors.clear();
	shapes.clear();

	std::ifstream ifs(OBJPath);
	if (!ifs.is_open()) 
	{
		throw std::runtime_error(std::format("Failed to open the OBJ file: {}", OBJPath));
	}

	if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &warn, &err, &ifs))
	{
		throw std::runtime_error(warn + err);
	}

	// Process the read OBJ file
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex
			{
				.pos =
				{
					attribute.vertices[3 * index.vertex_index + 0],
					attribute.vertices[3 * index.vertex_index + 1],
					attribute.vertices[3 * index.vertex_index + 2]
				},
				.normal =
				{
					attribute.normals[3 * index.normal_index + 0],
					attribute.normals[3 * index.normal_index + 1],
					attribute.normals[3 * index.normal_index + 2]
				},
				.texCoord =
				{
					attribute.texcoords[2 * index.texcoord_index + 0],
					1.0f - attribute.texcoords[2 * index.texcoord_index + 1] // Reflect the difference between the texture coordinate system
				}
			};

			indices.push_back(static_cast<uint32_t>(vertices.size()));
			vertices.push_back(vertex);
		}
	}

	return std::make_tuple(std::move(vertices), std::move(indices));
}

std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout)
{
	return CreateComputePipeline(logicalDevice, computeShaderModule, descriptorSetLayout, {});
}

std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges)
{
	VkPipelineShaderStageCreateInfo computeShaderStageInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = computeShaderModule,
		.pName = "main"
	};

	const VkPushConstantRange *pushConstantRangesPtr = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();
	VkPipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout,
		.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
		.pPushConstantRanges = pushConstantRangesPtr
	};

	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &computePipelineLayout))
	{
		throw std::runtime_error("Failed to create a compute pipeline layout.");
	}

	VkComputePipelineCreateInfo pipelineInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = computeShaderStageInfo,
		.layout = computePipelineLayout
	};

	VkPipeline computePipeline = VK_NULL_HANDLE;
	if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline))
	{
		throw std::runtime_error("Failed to create a compute pipeline.");
	}

	return std::make_tuple(computePipeline, computePipelineLayout);
}

uint32_t DivisionCeil(uint32_t x, uint32_t y)
{ 
	return (x + y - 1) / y; 
};