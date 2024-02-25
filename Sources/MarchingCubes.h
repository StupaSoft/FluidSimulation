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
#include "SimulationParameters.h"

struct MarchingCubesGrid
{
	glm::vec2 _xRange{};
	glm::vec2 _yRange{};
	glm::vec2 _zRange{};
	float _voxelInterval = 0.0f;
};

class MarchingCubes : public ComputeBase
{
private:
	struct ParticleProperty
	{
		alignas(4) uint32_t _particleCount = 0;
		alignas(4) float _kernelRadiusFactor = 0.0f;
		alignas(4) float _r1 = 0.0f;
		alignas(4) float _r2 = 0.0f;
		alignas(4) float _r3 = 0.0f;
	};

	struct MarchingCubesSetup
	{
		alignas(8) glm::vec2 _xRange{};
		alignas(8) glm::vec2 _yRange{};
		alignas(8) glm::vec2 _zRange{};
		alignas(4) float _voxelInterval = 1.0f;

		alignas(4) float _isovalue = 1500.0f;

		alignas(16) glm::uvec4 _voxelDimension{};
		alignas(4) uint32_t _voxelCount = 0;

		alignas(16) glm::uvec4 _cellDimension{};
		alignas(4) uint32_t _cellCount = 0;

		alignas(4) uint32_t _vertexCount = 0;
		alignas(4) uint32_t _indexCount = 0;
	};

	// Setup
	std::unique_ptr<ParticleProperty> _particleProperty = std::make_unique<ParticleProperty>();
	std::unique_ptr<MarchingCubesSetup> _setup = std::make_unique<MarchingCubesSetup>();

	// Compute pipeline
	VkPipeline _accumulationPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _accumulationPipelineLayout = VK_NULL_HANDLE;

	VkPipeline _constructionPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _constructionPipelineLayout = VK_NULL_HANDLE;

	VkPipeline _presentationPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _presentationPipelineLayout = VK_NULL_HANDLE;

	// Descriptor sets
	std::unique_ptr<DescriptorHelper> _descriptorHelper = nullptr;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

	VkDescriptorSetLayout _accumulationDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _accumulationDescriptorSets;

	VkDescriptorSetLayout _constructionDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _constructionDescriptorSets;

	VkDescriptorSetLayout _presentationDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _presentationDescriptorSets;

	// Basic buffers
	Buffer _particlePropertyBuffer;
	Buffer _setupBuffer;

	// Mesh construction buffers
	std::vector<Buffer> _particlePositionBuffers;
	Buffer _indexTableBuffer;
	Buffer _voxelBuffer;
	Buffer _vertexPositionBuffer; // Buffers that contains the vertex position result from the construction shader
	Buffer _normalBuffer;
	Buffer _indexBuffer;
	Buffer _vertexOutputBuffer; // Buffers that will be fed as the vertex buffer

	// Constants
	static const uint32_t MAX_SET_COUNT = 100;
	static const uint32_t CODES_COUNT = 256;
	static const uint32_t MAX_INDICES_IN_CELL = 15;
	static const std::vector<uint32_t> INDICES_TABLE;

public:
	MarchingCubes(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const SimulationParameters &simulationParameters, const MarchingCubesGrid &marchingCubesGrid);

	void UpdatePositions(const std::vector<glm::vec3> &positions);
	void UpdateParticleProperty(size_t particleCount, const SimulationParameters &simulationParameters);
	void UpdateGrid(const MarchingCubesGrid &parameters);

	float GetIsovalue() { return _setup->_isovalue; }
	void SetIsovalue(float isovalue);

	Buffer GetVertexBuffer() { return _vertexOutputBuffer; }
	Buffer GetIndexBuffer() { return _indexBuffer; }

	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, uint32_t currentFrame) override;
	virtual uint32_t GetOrder() override;

	uint32_t GetIndexCount() { return _setup->_indexCount; }

private:
	std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout);
	void CreateSetupBuffers();
	void CreateComputeBuffers(const MarchingCubesSetup &setup);

	VkDescriptorPool CreateDescriptorPool(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateAccumulationDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateConstructionDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreatePresentationDescriptors(DescriptorHelper *descriptorHelper);
};

