#pragma once

#include <algorithm>

#include "ComputeBase.h"
#include "DescriptorHelper.h"
#include "SimulationParameters.h"
#include "MathUtil.h"
#include "BVH.h"

class SimulationCompute : public ComputeBase
{
private:
	struct SimulationSetup
	{
		alignas(4) uint32_t _particleCount = 0;
	};

	struct GridSetup
	{
		// _gridSpacing is equal to 2.0f * _particleRadius * _kernelRadiusFactor
		alignas(16) glm::uvec4 _dimension{};
	};

	struct PrefixSumState
	{
		alignas(4) uint32_t _step = 0;
	};

private:
	uint32_t _prefixSumIterCount = 0;
	static const size_t OVERLAPPING_BUCKETS = 8;

	std::unique_ptr<SimulationSetup> _simulationSetup = std::make_unique<SimulationSetup>();
	std::unique_ptr<GridSetup> _gridSetup = std::make_unique<GridSetup>();
	std::unique_ptr<PrefixSumState> _prefixSumState = std::make_unique<PrefixSumState>();
	uint32_t _BVHMaxLevel = 0;

	// Setup buffer
	Buffer _simulationSetupBuffer = nullptr;
	Buffer _gridSetupBuffer = nullptr;
	Buffer _simulationParametersBuffer = nullptr;

	// Hashed grid buffer
	Buffer _hashResultBuffer = nullptr;
	Buffer _accumulationBuffer = nullptr;
	Buffer _bucketBuffer = nullptr;
	Buffer _adjacentBucketBuffer = nullptr;

	// Simulation buffers
	Buffer _positionBuffer = nullptr; // Input / output
	Buffer _densityBuffer = nullptr;
	Buffer _velocityBuffer = nullptr;
	Buffer _forceBuffer = nullptr; // Buffers that contains the vertex position result from the construction shader
	Buffer _pressureBuffer = nullptr;
	Buffer _nextPositionBuffer = nullptr;
	Buffer _nextVelocityBuffer = nullptr;
	Buffer _BVHStackBuffer = nullptr;
	Buffer _BVHNodeBuffer = nullptr;
	
	// Push constants
	VkPushConstantRange _prefixSumStatePushConstant{};
	VkPushConstantRange _BVHStatePushConstant{};

	// Descriptor sets
	std::unique_ptr<DescriptorHelper> _descriptorHelper = nullptr;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

	VkDescriptorSetLayout _hashingDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _hashingDescriptorSets;
	VkPipeline _hashingPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _hashingPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _prefixSumUpDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _prefixSumUpDescriptorSets;
	VkPipeline _prefixSumUpPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _prefixSumUpPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _prefixSumTurnPhaseDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _prefixSumTurnPhaseDescriptorSets;
	VkPipeline _prefixSumTurnPhasePipeline = VK_NULL_HANDLE;
	VkPipelineLayout _prefixSumTurnPhasePipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _prefixSumDownDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _prefixSumDownDescriptorSets;
	VkPipeline _prefixSumDownPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _prefixSumDownPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _countingSortDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _countingSortDescriptorSets;
	VkPipeline _countingSortPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _countingSortPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _densityDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _densityDescriptorSets;
	VkPipeline _densityPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _densityPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _externalForcesDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _externalForcesDescriptorSets;
	VkPipeline _externalForcesPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _externalForcesPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _computePressureDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _computePressureDescriptorSets;
	VkPipeline _computePressurePipeline = VK_NULL_HANDLE;
	VkPipelineLayout _computePressurePipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _pressureAndViscosityDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _pressureAndViscosityDescriptorSets;
	VkPipeline _pressureAndViscosityPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _pressureAndViscosityPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _timeIntegrationDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _timeIntegrationDescriptorSets;
	VkPipeline _timeIntegrationPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _timeIntegrationPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _resolveCollisionDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _resolveCollisionDescriptorSets;
	VkPipeline _resolveCollisionPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _resolveCollisionPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout _endTimeStepDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _endTimeStepDescriptorSets;
	VkPipeline _endTimeStepPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _endTimeStepPipelineLayout = VK_NULL_HANDLE;

public:
	SimulationCompute(glm::uvec3 gridDimension);
	virtual void Register() override;
	virtual ~SimulationCompute();

	void UpdateSimulationParameters(const SimulationParameters &simulationParameters);
	void InitializeLevel(const std::vector<BVH::Node> &BVHNodes);
	void InitializeParticles(const std::vector<glm::vec3> &positions);

	auto GetPositionInputBuffer() { return _positionBuffer; }

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) override;

private:
	void CreateSetupBuffers();
	void CreateGridBuffers(glm::uvec3 gridDimension);
	void CreateLevelBuffers(const std::vector<BVH::Node> &BVHNodes);
	void CreateSimulationBuffers(uint32_t particleCount, uint32_t BVHMaxLevel);

	void CreatePipelines(uint32_t particleCount, glm::uvec3 bucketDimension);

	VkDescriptorPool CreateDescriptorPool(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateHashingDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreatePrefixSumDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateCountingSortDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateDensityDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateExternalForcesDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateComputePressureDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreatePressureViscosityForceDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateTimeIntegrationDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateResolveCollisionDescriptors(DescriptorHelper *descriptorHelper);
	std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateEndTimeStepDescriptors(DescriptorHelper *descriptorHelper);

};
