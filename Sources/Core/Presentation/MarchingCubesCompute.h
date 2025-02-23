#pragma once

#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "ComputeBase.h"
#include "VulkanUtility.h"
#include "DescriptorHelper.h"
#include "Vertex.h"
#include "SimulationParameters.h"
#include "ShaderManager.h"

struct MarchingCubesGrid
{
	glm::vec2 _xRange{};
	glm::vec2 _yRange{};
	glm::vec2 _zRange{};
	float _voxelInterval = 0.0f;
};

class MarchingCubesCompute : public ComputeBase
{
private:
	struct ParticleProperty
	{
		alignas(4) uint32_t _particleCount = 0;
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
	};

	// Setup
	std::shared_ptr<ParticleProperty> _particleProperty = std::make_shared<ParticleProperty>();
	std::shared_ptr<MarchingCubesSetup> _setup = std::make_shared<MarchingCubesSetup>();

	// Basic buffers
	Buffer _particlePropertyBuffer = nullptr;
	Buffer _setupBuffer = nullptr;

	// Mesh construction buffers
	std::vector<Buffer> _particlePositionInputBuffers;
	Buffer _indexTableBuffer = nullptr;
	Buffer _voxelBuffer = nullptr;
	Buffer _indexBuffer = nullptr;
	Buffer _vertexBuffer = nullptr; // Buffers that will be fed as the vertex buffer
	Buffer _drawArgumentBuffer = nullptr;

	// Descriptor sets
	std::shared_ptr<DescriptorHelper> _descriptorHelper = nullptr;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

	// Compute pipeline
	VkDescriptorSetLayout _initializationDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _initializationDescriptorSets;
	VkPipeline _initializationPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _initializationPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _accumulationDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _accumulationDescriptorSets;
	VkPipeline _accumulationPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _accumulationPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _constructionDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _constructionDescriptorSets;
	VkPipeline _constructionPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _constructionPipelineLayout = VK_NULL_HANDLE;

	// Constants
	static const uint32_t CODES_COUNT = 256;
	static const uint32_t MAX_INDICES_IN_CELL = 15;
	static const std::vector<uint32_t> INDICES_TABLE;

public:
	MarchingCubesCompute(const std::vector<Buffer> &inputBuffers, size_t particleCount, const MarchingCubesGrid &marchingCubesGrid);
	virtual ~MarchingCubesCompute();

	void UpdateParticleProperty(const SimulationParameters &simulationParameters);

	float GetIsovalue() { return _setup->_isovalue; }
	void SetIsovalue(float isovalue);

	const std::vector<Buffer> &GetParticleInputBuffers() { return _particlePositionInputBuffers; }
	Buffer GetVertexBuffer() { return _vertexBuffer; }
	Buffer GetIndexBuffer() { return _indexBuffer; }
	Buffer GetDrawArgumentBuffer() { return _drawArgumentBuffer; }

protected:
	void InitializationGrid(const MarchingCubesGrid &parameters);

	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) override;

private:
	void CreateSetupBuffers();
	void CreateComputeBuffers(const MarchingCubesSetup &setup);

	VkDescriptorPool CreateDescriptorPool(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateInitializationDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateAccumulationDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateConstructionDescriptors(DescriptorHelper *descriptorHelper);
};

