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
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _hashingPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _hashingPipeline->GetPipelineLayout(), 0, 1, &_hashingDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
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
		vkCmdPushConstants(computeCommandBuffer, _prefixSumUpPipeline->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefixSumState), _prefixSumState.get());

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpPipeline->GetPipeline());
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpPipeline->GetPipelineLayout(), 0, 1, &_prefixSumDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, DivisionCeil(dispatchCount, 1024), 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	// Set the last element of accumulation buffer to zero.
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumTurnPhasePipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumTurnPhasePipeline->GetPipelineLayout(), 0, 1, &_prefixSumDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// Down-sweep phase
	for (uint32_t step = _prefixSumIterCount - 1; step != -1; --step)
	{
		_prefixSumState->_step = step;
		vkCmdPushConstants(computeCommandBuffer, _prefixSumDownPipeline->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefixSumState), _prefixSumState.get());

		uint32_t stride = 1 << (step + 1);
		uint32_t dispatchCount = bucketCount / stride;

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownPipeline->GetPipeline());
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownPipeline->GetPipelineLayout(), 0, 1, &_prefixSumDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, DivisionCeil(dispatchCount, 1024), 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	// 3. Counting sort of particles with their hash keys
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _countingSortPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _countingSortPipeline->GetPipelineLayout(), 0, 1, &_countingSortDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 4. Update densities
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _densityPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _densityPipeline->GetPipelineLayout(), 0, 1, &_densityDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 5. Accumulate external forces
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _externalForcesPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _externalForcesPipeline->GetPipelineLayout(), 0, 1, &_externalForcesDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 6. Compute pressures
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePressurePipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePressurePipeline->GetPipelineLayout(), 0, 1, &_computePressureDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 7. Accumulate pressure forces
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pressureAndViscosityPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pressureAndViscosityPipeline->GetPipelineLayout(), 0, 1, &_pressureAndViscosityDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 8. Time integration
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _timeIntegrationPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _timeIntegrationPipeline->GetPipelineLayout(), 0, 1, &_timeIntegrationDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 9. Resolve collision
	vkCmdPushConstants(computeCommandBuffer, _resolveCollisionPipeline->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &_BVHMaxLevel);

	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _resolveCollisionPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _resolveCollisionPipeline->GetPipelineLayout(), 0, 1, &_resolveCollisionDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	
	// 10. End a time step
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _endTimeStepPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _endTimeStepPipeline->GetPipelineLayout(), 0, 1, &_endTimeStepDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(std::max(_simulationSetup->_particleCount, bucketCount), 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}

void SimulationCompute::CreateSetupBuffers()
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_simulationSetupBuffer = CreateBuffer(sizeof(SimulationSetup), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_gridSetupBuffer = CreateBuffer(sizeof(GridSetup), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_simulationParametersBuffer = CreateBuffer(sizeof(SimulationParameters), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	memory->Bind({ _simulationSetupBuffer, _gridSetupBuffer, _simulationParametersBuffer });
}

void SimulationCompute::CreateGridBuffers(glm::uvec3 gridDimension)
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_accumulationBuffer = CreateBuffer(sizeof(uint32_t) * gridDimension.x * gridDimension.y * gridDimension.z, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _accumulationBuffer });
}

void SimulationCompute::CreateLevelBuffers(const std::vector<BVH::Node> &BVHNodes)
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_BVHNodeBuffer = CreateBuffer(sizeof(BVH::Node) * BVHNodes.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _BVHNodeBuffer });

	_BVHNodeBuffer->CopyFrom(BVHNodes.data());
}

void SimulationCompute::CreateSimulationBuffers(uint32_t particleCount, uint32_t BVHMaxLevel)
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
	Shader hashingShader = ShaderManager::Get()->GetShaderAsset("Hashing");
	_hashingDescriptor = CreateHashingDescriptors(hashingShader);
	_hashingPipeline = CreateComputePipeline(hashingShader->GetShaderModule(), _hashingDescriptor->GetDescriptorSetLayout());

	// Configure a push constant for prefix sum
	_prefixSumStatePushConstant.offset = 0;
	_prefixSumStatePushConstant.size = sizeof(PrefixSumState);
	_prefixSumStatePushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Shader prefixSumUpShader = ShaderManager::Get()->GetShaderAsset("PrefixSum", "mainUp");
	_prefixSumDescriptor = CreatePrefixSumDescriptors(prefixSumUpShader);
	_prefixSumUpPipeline = CreateComputePipeline(prefixSumUpShader->GetShaderModule(), _prefixSumDescriptor->GetDescriptorSetLayout(), { _prefixSumStatePushConstant });

	Shader prefixSumTurnPhaseShader = ShaderManager::Get()->GetShaderAsset("PrefixSum", "mainTurn");
	_prefixSumTurnPhasePipeline = CreateComputePipeline(prefixSumTurnPhaseShader->GetShaderModule(), _prefixSumDescriptor->GetDescriptorSetLayout());

	Shader prefixSumDownShader = ShaderManager::Get()->GetShaderAsset("PrefixSum", "mainDown");
	_prefixSumDownPipeline = CreateComputePipeline(prefixSumDownShader->GetShaderModule(), _prefixSumDescriptor->GetDescriptorSetLayout(), { _prefixSumStatePushConstant });

	Shader countingSortShader = ShaderManager::Get()->GetShaderAsset("CountingSort");
	_countingSortDescriptor = CreateCountingSortDescriptors(countingSortShader);
	_countingSortPipeline = CreateComputePipeline(countingSortShader->GetShaderModule(), _countingSortDescriptor->GetDescriptorSetLayout());

	Shader densityShader = ShaderManager::Get()->GetShaderAsset("UpdateDensity");
	_densityDescriptor = CreateDensityDescriptors(densityShader);
	_densityPipeline = CreateComputePipeline(densityShader->GetShaderModule(), _densityDescriptor->GetDescriptorSetLayout());

	Shader externalForcesShader = ShaderManager::Get()->GetShaderAsset("AccumulateExternalForces");
	_externalForcesDescriptor = CreateExternalForcesDescriptors(externalForcesShader);
	_externalForcesPipeline = CreateComputePipeline(externalForcesShader->GetShaderModule(), _externalForcesDescriptor->GetDescriptorSetLayout());

	Shader computePressureShader = ShaderManager::Get()->GetShaderAsset("ComputePressure");
	_computePressureDescriptor = CreateComputePressureDescriptors(computePressureShader);
	_computePressurePipeline = CreateComputePipeline(computePressureShader->GetShaderModule(), _computePressureDescriptor->GetDescriptorSetLayout());

	Shader pressureAndViscosityShader = ShaderManager::Get()->GetShaderAsset("AccumulatePressureAndViscosity");
	_pressureAndViscosityDescriptor = CreatePressureViscosityForceDescriptors(pressureAndViscosityShader);
	_pressureAndViscosityPipeline = CreateComputePipeline(pressureAndViscosityShader->GetShaderModule(), _pressureAndViscosityDescriptor->GetDescriptorSetLayout());

	Shader timeIntegrationShader = ShaderManager::Get()->GetShaderAsset("TimeIntegration");
	_timeIntegrationDescriptor = CreateTimeIntegrationDescriptors(timeIntegrationShader);
	_timeIntegrationPipeline = CreateComputePipeline(timeIntegrationShader->GetShaderModule(), _timeIntegrationDescriptor->GetDescriptorSetLayout());

	// Configure a push constant for prefix sum
	_BVHStatePushConstant.offset = 0;
	_BVHStatePushConstant.size = sizeof(uint32_t);
	_BVHStatePushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Shader resolveCollisionShader = ShaderManager::Get()->GetShaderAsset("ResolveCollision");
	_resolveCollisionDescriptor = CreateResolveCollisionDescriptors(resolveCollisionShader);
	_resolveCollisionPipeline = CreateComputePipeline(resolveCollisionShader->GetShaderModule(), _resolveCollisionDescriptor->GetDescriptorSetLayout(), {_BVHStatePushConstant});

	Shader endTimeStepShader = ShaderManager::Get()->GetShaderAsset("EndTimeStep");
	_endTimeStepDescriptor = CreateEndTimeStepDescriptors(endTimeStepShader);
	_endTimeStepPipeline = CreateComputePipeline(endTimeStepShader->GetShaderModule(), _endTimeStepDescriptor->GetDescriptorSetLayout());
}

Descriptor SimulationCompute::CreateHashingDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("gridSetup", _gridSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("accumulations", _accumulationBuffer);
	descriptor->BindBuffer("hashResults", _hashResultBuffer);
	descriptor->BindBuffer("adjacentBuckets", _adjacentBucketBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreatePrefixSumDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("gridSetup", _gridSetupBuffer);
	descriptor->BindBuffer("accumulations", _accumulationBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateCountingSortDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("hashResults", _hashResultBuffer);
	descriptor->BindBuffer("accumulations", _accumulationBuffer);
	descriptor->BindBuffer("buckets", _bucketBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateDensityDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	// Create descriptor sets
	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("gridSetup", _gridSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("hashResults", _hashResultBuffer);
	descriptor->BindBuffer("accumulations", _accumulationBuffer);
	descriptor->BindBuffer("buckets", _bucketBuffer);
	descriptor->BindBuffer("adjacentBuckets", _adjacentBucketBuffer);
	descriptor->BindBuffer("densities", _densityBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateExternalForcesDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	// Create descriptor sets
	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("velocities", _velocityBuffer);
	descriptor->BindBuffer("forces", _forceBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateComputePressureDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("densities", _densityBuffer);
	descriptor->BindBuffer("pressures", _pressureBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreatePressureViscosityForceDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("gridSetup", _gridSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("hashResults", _hashResultBuffer);
	descriptor->BindBuffer("accumulations", _accumulationBuffer);
	descriptor->BindBuffer("buckets", _bucketBuffer);
	descriptor->BindBuffer("adjacentBuckets", _adjacentBucketBuffer);
	descriptor->BindBuffer("velocities", _velocityBuffer);
	descriptor->BindBuffer("densities", _densityBuffer);
	descriptor->BindBuffer("pressures", _pressureBuffer);
	descriptor->BindBuffer("forces", _forceBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateTimeIntegrationDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	// Create descriptor sets
	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("velocities", _velocityBuffer);
	descriptor->BindBuffer("forces", _forceBuffer);
	descriptor->BindBuffer("nextVelocities", _nextVelocityBuffer);
	descriptor->BindBuffer("nextPositions", _nextPositionBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateResolveCollisionDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("simulationParameters", _simulationParametersBuffer);
	descriptor->BindBuffer("nodes", _BVHNodeBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("velocities", _velocityBuffer);
	descriptor->BindBuffer("nodeStack", _BVHStackBuffer);
	descriptor->BindBuffer("nextVelocities", _nextVelocityBuffer);
	descriptor->BindBuffer("nextPositions", _nextPositionBuffer);

	return descriptor;
}

Descriptor SimulationCompute::CreateEndTimeStepDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("simulationSetup", _simulationSetupBuffer);
	descriptor->BindBuffer("gridSetup", _gridSetupBuffer);
	descriptor->BindBuffer("nextVelocities", _nextVelocityBuffer);
	descriptor->BindBuffer("nextPositions", _nextPositionBuffer);
	descriptor->BindBuffer("positions", _positionBuffer);
	descriptor->BindBuffer("velocities", _velocityBuffer);
	descriptor->BindBuffer("forces", _forceBuffer);
	descriptor->BindBuffer("densities", _densityBuffer);
	descriptor->BindBuffer("pressures", _pressureBuffer);
	descriptor->BindBuffer("accumulations", _accumulationBuffer);

	return descriptor;
}
