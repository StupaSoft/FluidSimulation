#pragma once

#include <algorithm>

#include "ComputeBase.h"
#include "Descriptor.h"
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
	
	Descriptor _hashingDescriptor = nullptr;
	Pipeline _hashingPipeline = nullptr;

	Descriptor _prefixSumDescriptor = nullptr;
	Pipeline _prefixSumUpPipeline = nullptr;
	Pipeline _prefixSumTurnPhasePipeline = nullptr;
	Pipeline _prefixSumDownPipeline = nullptr;

	Descriptor _countingSortDescriptor = nullptr;
	Pipeline _countingSortPipeline = nullptr;

	Descriptor _densityDescriptor = nullptr;
	Pipeline _densityPipeline = nullptr;

	Descriptor _externalForcesDescriptor = nullptr;
	Pipeline _externalForcesPipeline = nullptr;

	Descriptor _computePressureDescriptor = nullptr;
	Pipeline _computePressurePipeline = nullptr;

	Descriptor _pressureAndViscosityDescriptor = nullptr;
	Pipeline _pressureAndViscosityPipeline = nullptr;

	Descriptor _timeIntegrationDescriptor = nullptr;
	Pipeline _timeIntegrationPipeline = nullptr;

	Descriptor _resolveCollisionDescriptor = nullptr;
	Pipeline _resolveCollisionPipeline = nullptr;

	Descriptor _endTimeStepDescriptor = nullptr;
	Pipeline _endTimeStepPipeline = nullptr;

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

	Descriptor CreateHashingDescriptors(const Shader &shader);
	Descriptor CreatePrefixSumDescriptors(const Shader &shader);
	Descriptor CreateCountingSortDescriptors(const Shader &shader);
	Descriptor CreateDensityDescriptors(const Shader &shader);
	Descriptor CreateExternalForcesDescriptors(const Shader &shader);
	Descriptor CreateComputePressureDescriptors(const Shader &shader);
	Descriptor CreatePressureViscosityForceDescriptors(const Shader &shader);
	Descriptor CreateTimeIntegrationDescriptors(const Shader &shader);
	Descriptor CreateResolveCollisionDescriptors(const Shader &shader);
	Descriptor CreateEndTimeStepDescriptors(const Shader &shader);

};
