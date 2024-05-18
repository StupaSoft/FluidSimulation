#include "SimulationCompute.h"

SimulationCompute::SimulationCompute(glm::uvec3 gridDimension)
{	
	// Prepare setups
	CreateSetupBuffers(); // Create setup buffers in the constructor since their size does not count on the number of particles.
	CreateGridBuffers(gridDimension);

	_gridSetup->_dimension = glm::uvec4(gridDimension, 0);
	_gridSetupBuffer->CopyFrom(_gridSetup.get());

	uint32_t bucketCount = _gridSetup->_dimension.x * _gridSetup->_dimension.y * _gridSetup->_dimension.z;
	_prefixSumIterCount = Log(bucketCount); // log(2, n) - 1

	// Create descriptor pool and sets
	_descriptorHelper = std::make_unique<DescriptorHelper>();
	_descriptorPool = CreateDescriptorPool(_descriptorHelper.get());
}

void SimulationCompute::Register()
{
	ComputeBase::Register();

	// Do not play immediately
	// Play only after the simulation parameters are set and particles are initialized
	SetEnable(false);
}

SimulationCompute::~SimulationCompute()
{
	vkDeviceWaitIdle(VulkanCore::Get()->GetLogicalDevice());

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _hashingPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _hashingPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _hashingDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _prefixSumUpPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _prefixSumUpPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _prefixSumUpDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _prefixSumTurnPhasePipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _prefixSumTurnPhasePipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _prefixSumTurnPhaseDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _prefixSumDownPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _prefixSumDownPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _prefixSumDownDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _countingSortPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _countingSortPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _countingSortDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _densityPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _densityPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _densityDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _externalForcesPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _externalForcesPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _externalForcesDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _computePressurePipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _computePressurePipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _computePressureDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _pressureAndViscosityPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _pressureAndViscosityPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _pressureAndViscosityDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _timeIntegrationPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _timeIntegrationPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _timeIntegrationDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _resolveCollisionPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _resolveCollisionPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _resolveCollisionDescriptorSetLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _endTimeStepPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _endTimeStepPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _endTimeStepDescriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), _descriptorPool, nullptr);
}

void SimulationCompute::UpdateSimulationParameters(const SimulationParameters &simulationParameters)
{
	_simulationParametersBuffer->CopyFrom(&simulationParameters);
}

void SimulationCompute::InitializeLevel(const std::vector<BVH::Node> &BVHNodes)
{
	auto maxLevelIter = std::max_element(BVHNodes.cbegin(), BVHNodes.cend(), [](const BVH::Node &node1, const BVH::Node &node2) { return node1._level < node2._level; });
	_BVHMaxLevel = maxLevelIter->_level;

	CreateLevelBuffers(BVHNodes);
}

void SimulationCompute::InitializeParticles(const std::vector<glm::vec3> &positions)
{
	// Populate setups
	_simulationSetup->_particleCount = static_cast<uint32_t>(positions.size());

	// Create resources
	CreateSimulationBuffers(_simulationSetup->_particleCount, _BVHMaxLevel);
	CreatePipelines(_simulationSetup->_particleCount, _gridSetup->_dimension);

	// Transfer simulation setup
	_simulationSetupBuffer->CopyFrom(_simulationSetup.get());

	// Finally copy the particle positions
	_positionBuffer->CopyFrom(positions.data());
}

void SimulationCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame)
{
	VkMemoryBarrier memoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
	};

	// 1. Hash particle positions and yield counts for each bucket
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _hashingPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _hashingPipelineLayout, 0, 1, &_hashingDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 2. Prefix sum of bucket counts
	// Up-sweep phase
	uint32_t bucketCount = _gridSetup->_dimension.x * _gridSetup->_dimension.y * _gridSetup->_dimension.z;

	for (uint32_t step = 0; step < _prefixSumIterCount; ++step)
	{
		uint32_t stride = 1 << (step + 1); // This will also be kept by the shader internally.
		uint32_t dispatchCount = bucketCount / stride;

		_prefixSumState->_step = step;
		vkCmdPushConstants(computeCommandBuffer, _prefixSumUpPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefixSumState), _prefixSumState.get());

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpPipeline);
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpPipelineLayout, 0, 1, &_prefixSumUpDescriptorSets[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, DivisionCeil(dispatchCount, 1024), 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	// Set the last element of accumulation buffer to zero.
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumTurnPhasePipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumTurnPhasePipelineLayout, 0, 1, &_prefixSumTurnPhaseDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// Down-sweep phase
	for (uint32_t step = _prefixSumIterCount - 1; step != -1; --step)
	{
		_prefixSumState->_step = step;
		vkCmdPushConstants(computeCommandBuffer, _prefixSumDownPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefixSumState), _prefixSumState.get());

		uint32_t stride = 1 << (step + 1);
		uint32_t dispatchCount = bucketCount / stride;

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownPipeline);
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownPipelineLayout, 0, 1, &_prefixSumDownDescriptorSets[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, DivisionCeil(dispatchCount, 1024), 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	// 3. Counting sort of particles with their hash keys
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _countingSortPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _countingSortPipelineLayout, 0, 1, &_countingSortDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 4. Update densities
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _densityPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _densityPipelineLayout, 0, 1, &_densityDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 5. Accumulate external forces
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _externalForcesPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _externalForcesPipelineLayout, 0, 1, &_externalForcesDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 6. Compute pressures
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePressurePipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePressurePipelineLayout, 0, 1, &_computePressureDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 7. Accumulate pressure forces
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pressureAndViscosityPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pressureAndViscosityPipelineLayout, 0, 1, &_pressureAndViscosityDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 8. Time integration
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _timeIntegrationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _timeIntegrationPipelineLayout, 0, 1, &_timeIntegrationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 9. Resolve collision
	vkCmdPushConstants(computeCommandBuffer, _resolveCollisionPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &_BVHMaxLevel);

	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _resolveCollisionPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _resolveCollisionPipelineLayout, 0, 1, &_resolveCollisionDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	
	// 10. End a time step
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _endTimeStepPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _endTimeStepPipelineLayout, 0, 1, &_endTimeStepDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(std::max(_simulationSetup->_particleCount, bucketCount), 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}

void SimulationCompute::CreateSetupBuffers()
{
	Memory memory = std::make_shared<DeviceMemory>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_simulationSetupBuffer = CreateBuffer(sizeof(SimulationSetup), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_gridSetupBuffer = CreateBuffer(sizeof(GridSetup), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_simulationParametersBuffer = CreateBuffer(sizeof(SimulationParameters), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _simulationSetupBuffer, _gridSetupBuffer, _simulationParametersBuffer });
}

void SimulationCompute::CreateGridBuffers(glm::uvec3 gridDimension)
{
	Memory memory = std::make_shared<DeviceMemory>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_accumulationBuffer = CreateBuffer(sizeof(uint32_t) * gridDimension.x * gridDimension.y * gridDimension.z, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _accumulationBuffer });
}

void SimulationCompute::CreateLevelBuffers(const std::vector<BVH::Node> &BVHNodes)
{
	Memory memory = std::make_shared<DeviceMemory>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_BVHNodeBuffer = CreateBuffer(sizeof(BVH::Node) * BVHNodes.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _BVHNodeBuffer });

	_BVHNodeBuffer->CopyFrom(BVHNodes.data());
}

void SimulationCompute::CreateSimulationBuffers(uint32_t particleCount, uint32_t BVHMaxLevel)
{
	Memory memory = std::make_shared<DeviceMemory>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_hashResultBuffer = CreateBuffer(sizeof(uint32_t) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_adjacentBucketBuffer = CreateBuffer(sizeof(uint32_t) * particleCount * OVERLAPPING_BUCKETS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_bucketBuffer = CreateBuffer(sizeof(uint32_t) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_positionBuffer = CreateBuffer(sizeof(glm::vec3) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	_densityBuffer = CreateBuffer(sizeof(float) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_velocityBuffer = CreateBuffer(sizeof(glm::vec3) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_forceBuffer = CreateBuffer(sizeof(glm::vec3) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_pressureBuffer = CreateBuffer(sizeof(glm::vec3) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_BVHStackBuffer = CreateBuffer(sizeof(uint32_t) * particleCount * BVHMaxLevel, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_nextPositionBuffer = CreateBuffer(sizeof(glm::vec3) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_nextVelocityBuffer = CreateBuffer(sizeof(glm::vec3) * particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _hashResultBuffer, _adjacentBucketBuffer, _bucketBuffer, _positionBuffer, _densityBuffer, _velocityBuffer, _forceBuffer, _pressureBuffer, _BVHStackBuffer, _nextPositionBuffer, _nextVelocityBuffer });
}

void SimulationCompute::CreatePipelines(uint32_t particleCount, glm::uvec3 bucketDimension)
{
	VkShaderModule hashingShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/Hashing.spv"));
	std::tie(_hashingDescriptorSetLayout, _hashingDescriptorSets) = CreateHashingDescriptors(_descriptorHelper.get());
	std::tie(_hashingPipeline, _hashingPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), hashingShaderModule, _hashingDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), hashingShaderModule, nullptr);

	// Configure a push constant for prefix sum
	_prefixSumStatePushConstant.offset = 0;
	_prefixSumStatePushConstant.size = sizeof(PrefixSumState);
	_prefixSumStatePushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkShaderModule prefixSumUpShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/PrefixSumUp.spv"));
	std::tie(_prefixSumUpDescriptorSetLayout, _prefixSumUpDescriptorSets) = CreatePrefixSumDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumUpPipeline, _prefixSumUpPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), prefixSumUpShaderModule, _prefixSumUpDescriptorSetLayout, { _prefixSumStatePushConstant });
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), prefixSumUpShaderModule, nullptr);

	VkShaderModule prefixSumTurnPhaseShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/PrefixSumTurnPhase.spv"));
	std::tie(_prefixSumTurnPhaseDescriptorSetLayout, _prefixSumTurnPhaseDescriptorSets) = CreatePrefixSumTurnPhaseDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumTurnPhasePipeline, _prefixSumTurnPhasePipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), prefixSumTurnPhaseShaderModule, _prefixSumTurnPhaseDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), prefixSumTurnPhaseShaderModule, nullptr);

	VkShaderModule prefixSumDownShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/PrefixSumDown.spv"));
	std::tie(_prefixSumDownDescriptorSetLayout, _prefixSumDownDescriptorSets) = CreatePrefixSumDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumDownPipeline, _prefixSumDownPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), prefixSumDownShaderModule, _prefixSumDownDescriptorSetLayout, { _prefixSumStatePushConstant });
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), prefixSumDownShaderModule, nullptr);

	VkShaderModule countingSortShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/CountingSort.spv"));
	std::tie(_countingSortDescriptorSetLayout, _countingSortDescriptorSets) = CreateCountingSortDescriptors(_descriptorHelper.get());
	std::tie(_countingSortPipeline, _countingSortPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), countingSortShaderModule, _countingSortDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), countingSortShaderModule, nullptr);

	VkShaderModule updateDensityShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/UpdateDensity.spv"));
	std::tie(_densityDescriptorSetLayout, _densityDescriptorSets) = CreateDensityDescriptors(_descriptorHelper.get());
	std::tie(_densityPipeline, _densityPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), updateDensityShaderModule, _densityDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), updateDensityShaderModule, nullptr);

	VkShaderModule externalForcesShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/AccumulateExternalForces.spv"));
	std::tie(_externalForcesDescriptorSetLayout, _externalForcesDescriptorSets) = CreateExternalForcesDescriptors(_descriptorHelper.get());
	std::tie(_externalForcesPipeline, _externalForcesPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), externalForcesShaderModule, _externalForcesDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), externalForcesShaderModule, nullptr);

	VkShaderModule computePressureShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/ComputePressure.spv"));
	std::tie(_computePressureDescriptorSetLayout, _computePressureDescriptorSets) = CreateComputePressureDescriptors(_descriptorHelper.get());
	std::tie(_computePressurePipeline, _computePressurePipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), computePressureShaderModule, _computePressureDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), computePressureShaderModule, nullptr);

	VkShaderModule pressureAndViscosity = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/AccumulatePressureAndViscosity.spv"));
	std::tie(_pressureAndViscosityDescriptorSetLayout, _pressureAndViscosityDescriptorSets) = CreatePressureForceDescriptors(_descriptorHelper.get());
	std::tie(_pressureAndViscosityPipeline, _pressureAndViscosityPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), pressureAndViscosity, _pressureAndViscosityDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), pressureAndViscosity, nullptr);

	VkShaderModule timeIntegrationShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/TimeIntegration.spv"));
	std::tie(_timeIntegrationDescriptorSetLayout, _timeIntegrationDescriptorSets) = CreateTimeIntegrationDescriptors(_descriptorHelper.get());
	std::tie(_timeIntegrationPipeline, _timeIntegrationPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), timeIntegrationShaderModule, _timeIntegrationDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), timeIntegrationShaderModule, nullptr);

	// Configure a push constant for prefix sum
	_BVHStatePushConstant.offset = 0;
	_BVHStatePushConstant.size = sizeof(uint32_t);
	_BVHStatePushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkShaderModule resolveCollisionShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/ResolveCollision.spv"));
	std::tie(_resolveCollisionDescriptorSetLayout, _resolveCollisionDescriptorSets) = CreateResolveCollisionDescriptors(_descriptorHelper.get());
	std::tie(_resolveCollisionPipeline, _resolveCollisionPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), resolveCollisionShaderModule, _resolveCollisionDescriptorSetLayout, { _BVHStatePushConstant });
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), resolveCollisionShaderModule, nullptr);

	VkShaderModule endTimeStepShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(L"Shaders/Simulation/EndTimeStep.spv"));
	std::tie(_endTimeStepDescriptorSetLayout, _endTimeStepDescriptorSets) = CreateEndTimeStepDescriptors(_descriptorHelper.get());
	std::tie(_endTimeStepPipeline, _endTimeStepPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), endTimeStepShaderModule, _endTimeStepDescriptorSetLayout);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), endTimeStepShaderModule, nullptr);
}

VkDescriptorPool SimulationCompute::CreateDescriptorPool(DescriptorHelper *descriptorHelper)
{
	// Create descriptor pool
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 });
	auto descriptorPool = descriptorHelper->GetDescriptorPool();

	return descriptorPool;
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateHashingDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _hashResultBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _adjacentBucketBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _gridSetupBuffer);
	descriptorHelper->BindBuffer(2, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(3, _positionBuffer);
	descriptorHelper->BindBuffer(4, _accumulationBuffer);
	descriptorHelper->BindBuffer(5, _hashResultBuffer);
	descriptorHelper->BindBuffer(6, _adjacentBucketBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreatePrefixSumDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _gridSetupBuffer);
	descriptorHelper->BindBuffer(1, _accumulationBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreatePrefixSumTurnPhaseDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _gridSetupBuffer);
	descriptorHelper->BindBuffer(1, _accumulationBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateCountingSortDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _hashResultBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _bucketBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _hashResultBuffer);
	descriptorHelper->BindBuffer(2, _accumulationBuffer);
	descriptorHelper->BindBuffer(3, _bucketBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateDensityDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _hashResultBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _bucketBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _adjacentBucketBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _densityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _gridSetupBuffer);
	descriptorHelper->BindBuffer(2, _positionBuffer);
	descriptorHelper->BindBuffer(3, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(4, _hashResultBuffer);
	descriptorHelper->BindBuffer(5, _accumulationBuffer);
	descriptorHelper->BindBuffer(6, _bucketBuffer);
	descriptorHelper->BindBuffer(7, _adjacentBucketBuffer);
	descriptorHelper->BindBuffer(8, _densityBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateExternalForcesDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _velocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _forceBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _gridSetupBuffer);
	descriptorHelper->BindBuffer(2, _positionBuffer);
	descriptorHelper->BindBuffer(3, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(4, _velocityBuffer);
	descriptorHelper->BindBuffer(5, _forceBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateComputePressureDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _densityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _pressureBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _gridSetupBuffer);
	descriptorHelper->BindBuffer(2, _positionBuffer);
	descriptorHelper->BindBuffer(3, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(4, _densityBuffer);
	descriptorHelper->BindBuffer(5, _pressureBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreatePressureForceDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _hashResultBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _bucketBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _adjacentBucketBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _velocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(9, _densityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(10, _pressureBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(11, _forceBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _gridSetupBuffer);
	descriptorHelper->BindBuffer(2, _positionBuffer);
	descriptorHelper->BindBuffer(3, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(4, _hashResultBuffer);
	descriptorHelper->BindBuffer(5, _accumulationBuffer);
	descriptorHelper->BindBuffer(6, _bucketBuffer);
	descriptorHelper->BindBuffer(7, _adjacentBucketBuffer);
	descriptorHelper->BindBuffer(8, _velocityBuffer);
	descriptorHelper->BindBuffer(9, _densityBuffer);
	descriptorHelper->BindBuffer(10, _pressureBuffer);
	descriptorHelper->BindBuffer(11, _forceBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateTimeIntegrationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _velocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _forceBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _nextVelocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _nextPositionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(2, _positionBuffer);
	descriptorHelper->BindBuffer(3, _velocityBuffer);
	descriptorHelper->BindBuffer(4, _forceBuffer);
	descriptorHelper->BindBuffer(5, _nextVelocityBuffer);
	descriptorHelper->BindBuffer(6, _nextPositionBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateResolveCollisionDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _simulationParametersBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _BVHNodeBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _velocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _BVHStackBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _nextVelocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _nextPositionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _simulationParametersBuffer);
	descriptorHelper->BindBuffer(2, _BVHNodeBuffer);
	descriptorHelper->BindBuffer(3, _positionBuffer);
	descriptorHelper->BindBuffer(4, _velocityBuffer);
	descriptorHelper->BindBuffer(5, _BVHStackBuffer);
	descriptorHelper->BindBuffer(6, _nextVelocityBuffer);
	descriptorHelper->BindBuffer(7, _nextPositionBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateEndTimeStepDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _nextVelocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _nextPositionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _positionBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _velocityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _forceBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _densityBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _pressureBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(9, _accumulationBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _simulationSetupBuffer);
	descriptorHelper->BindBuffer(1, _gridSetupBuffer);
	descriptorHelper->BindBuffer(2, _nextVelocityBuffer);
	descriptorHelper->BindBuffer(3, _nextPositionBuffer);
	descriptorHelper->BindBuffer(4, _positionBuffer);
	descriptorHelper->BindBuffer(5, _velocityBuffer);
	descriptorHelper->BindBuffer(6, _forceBuffer);
	descriptorHelper->BindBuffer(7, _densityBuffer);
	descriptorHelper->BindBuffer(8, _pressureBuffer);
	descriptorHelper->BindBuffer(9, _accumulationBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}
