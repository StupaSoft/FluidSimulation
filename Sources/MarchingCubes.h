#pragma once

#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ComputeBase.h"
#include "VulkanUtility.h"
#include "DescriptorHelper.h"
#include "Vertex.h"

struct MarchingCubesSetup
{
	alignas(8) glm::vec2 xRange{};
	alignas(8) glm::vec2 yRange{};
	alignas(8) glm::vec2 zRange{};

	alignas(4) float voxelInterval = 0.0f;
};

class MarchingCubes : public ComputeBase
{
private:
	// Compute pipeline
	VkPipeline _computePipeline = VK_NULL_HANDLE;
	VkPipelineLayout _computePipelineLayout = VK_NULL_HANDLE;

	// Descriptor sets
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _descriptorSets;

	// Input particle position buffers
	std::vector<VkBuffer> _positionBuffers;
	std::vector<VkDeviceMemory> _positionBufferMemory;

	// Marching cubes mesh buffers
	std::vector<VkBuffer> _setupBuffers;
	std::vector<VkDeviceMemory> _setupBufferMemory;

	std::vector<VkBuffer> _vertexBuffers;
	std::vector<VkDeviceMemory> _vertexBufferMemory;

	std::vector<VkBuffer> _indexBuffers;
	std::vector<VkDeviceMemory> _indexBufferMemory;
	
	uint32_t _vertexCount = 0;
	uint32_t _indexCount = 0;

	static const uint32_t MAX_SET_COUNT = 100;

	static const uint32_t MAX_INDICES_IN_CELL = 15;
	static const glm::uvec3 WORK_GROUP_DIMENSION; // Local size of the work group

public:
	MarchingCubes(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const MarchingCubesSetup &setup);

	void UpdateSetup(const MarchingCubesSetup &setup);
	void UpdatePositions(const std::vector<glm::vec3> &positions);

	virtual void RecordCommand(VkCommandBuffer commandBuffer, VkCommandBuffer computeCommandBuffer, uint32_t currentFrame) override;
	virtual uint32_t GetOrder() override;

private:
	std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDescriptorSetLayout descriptorSetLayout);
	void CreateMeshBuffers(const MarchingCubesSetup &setup);

	std::tuple<VkDescriptorPool, VkDescriptorSetLayout, std::vector<VkDescriptorSet>> PrepareDescriptors();
};

