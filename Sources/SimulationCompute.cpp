#include "SimulationCompute.h"

SimulationCompute::SimulationCompute(const std::shared_ptr<VulkanCore> &vulkanCore, glm::uvec3 gridDimension) :
	ComputeBase(vulkanCore)
{	
	// Prepare setups
	CreateSetupBuffers(); // Create setup buffers in the constructor since their size does not count on the number of particles.
	CreateGridBuffers(gridDimension);

	_gridSetup->_dimension = glm::uvec4(gridDimension, 0);
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _gridSetup.get(), _gridSetupBuffer, 0);

	uint32_t bucketCount = _gridSetup->_dimension.x * _gridSetup->_dimension.y * _gridSetup->_dimension.z;
	uint32_t tempBucketCount = bucketCount;
	uint32_t iterCount = 0;
	while (tempBucketCount >>= 1) ++iterCount;
	_prefixSumState->_iterCount = iterCount; // log(2, n - 1)
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _prefixSumState.get(), _prefixSumStateBuffer, 0);

	// Create descriptor pool and sets
	_descriptorHelper = std::make_unique<DescriptorHelper>(_vulkanCore);
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
}

void SimulationCompute::UpdateSimulationParameters(const SimulationParameters &simulationParameters)
{
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), &simulationParameters, _simulationParametersBuffer, 0);
}

void SimulationCompute::InitializeParticles(const std::vector<glm::vec3> &positions)
{
	// Populate setups
	_simulationSetup->_particleCount = static_cast<uint32_t>(positions.size());

	// Create resources
	CreateSimulationBuffers(_simulationSetup->_particleCount);
	CreatePipelines(_simulationSetup->_particleCount, _gridSetup->_dimension);

	// Transfer simulation setup
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _simulationSetup.get(), _simulationSetupBuffer, 0);

	// Finally copy the particle positions
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), positions.data(), _positionBuffer, 0);
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

	for (uint32_t step = 0; step < _prefixSumState->_iterCount; ++step)
	{
		uint32_t stride = 1 << (step + 1); // This will also be kept by the shader internally.
		uint32_t dispatchCount = bucketCount / stride;

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpPipeline);
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpPipelineLayout, 0, 1, &_prefixSumUpDescriptorSets[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, DivisionCeil(dispatchCount, 1024), 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpStatePipeline);
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumUpStatePipelineLayout, 0, 1, &_prefixSumUpStateDescriptorSets[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	// Set the last element of accumulation buffer to zero.
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumTurnPhasePipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumTurnPhasePipelineLayout, 0, 1, &_prefixSumTurnPhaseDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// Down-sweep phase
	for (uint32_t step = _prefixSumState->_iterCount - 1; step != -1; --step)
	{
		uint32_t stride = 1 << (step + 1); // This will also be kept by the shader internally.
		uint32_t dispatchCount = bucketCount / stride;

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownPipeline);
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownPipelineLayout, 0, 1, &_prefixSumDownDescriptorSets[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, DivisionCeil(dispatchCount, 1024), 1, 1);

		vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownStatePipeline);
		vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _prefixSumDownStatePipelineLayout, 0, 1, &_prefixSumDownStateDescriptorSets[currentFrame], 0, 0);
		vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

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

	// 6. Accumulate viscosity forces
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _viscosityPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _viscosityPipelineLayout, 0, 1, &_viscosityDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 7. Compute pressures
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePressurePipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePressurePipelineLayout, 0, 1, &_computePressureDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 8. Accumulate pressure forces
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pressureForcePipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pressureForcePipelineLayout, 0, 1, &_pressureForceDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_simulationSetup->_particleCount, 1024), 1, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 9. Time integration
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _timeIntegrationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _timeIntegrationPipelineLayout, 0, 1, &_timeIntegrationDescriptorSets[currentFrame], 0, 0);
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
	_simulationSetupBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(SimulationSetup),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_gridSetupBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(GridSetup),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_simulationParametersBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(SimulationParameters),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_prefixSumStateBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(PrefixSumState),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
}

void SimulationCompute::CreateGridBuffers(glm::uvec3 gridDimension)
{
	_adjacentBucketBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * gridDimension.x * gridDimension.y * gridDimension.z * OVERLAPPING_BUCKETS,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_accumulationBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * gridDimension.x * gridDimension.y * gridDimension.z,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
}

void SimulationCompute::CreateSimulationBuffers(uint32_t particleCount)
{
	_hashResultBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_bucketBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_positionBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_densityBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(float) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_velocityBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_forceBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_pressureBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_nextPositionBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_nextVelocityBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
}

void SimulationCompute::CreatePipelines(uint32_t particleCount, glm::uvec3 bucketDimension)
{
	VkShaderModule hashingShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/Hashing.spv"));
	std::tie(_hashingDescriptorSetLayout, _hashingDescriptorSets) = CreateHashingDescriptors(_descriptorHelper.get());
	std::tie(_hashingPipeline, _hashingPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), hashingShaderModule, _hashingDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), hashingShaderModule, nullptr);

	VkShaderModule prefixSumUpShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/PrefixSumUp.spv"));
	std::tie(_prefixSumUpDescriptorSetLayout, _prefixSumUpDescriptorSets) = CreatePrefixSumDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumUpPipeline, _prefixSumUpPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), prefixSumUpShaderModule, _prefixSumUpDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), prefixSumUpShaderModule, nullptr);

	VkShaderModule prefixSumUpStateShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/PrefixSumUpState.spv"));
	std::tie(_prefixSumUpStateDescriptorSetLayout, _prefixSumUpStateDescriptorSets) = CreatePrefixSumStateDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumUpStatePipeline, _prefixSumUpStatePipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), prefixSumUpStateShaderModule, _prefixSumUpStateDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), prefixSumUpStateShaderModule, nullptr);

	VkShaderModule prefixSumTurnPhaseShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/PrefixSumTurnPhase.spv"));
	std::tie(_prefixSumTurnPhaseDescriptorSetLayout, _prefixSumTurnPhaseDescriptorSets) = CreatePrefixSumTurnPhaseDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumTurnPhasePipeline, _prefixSumTurnPhasePipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), prefixSumTurnPhaseShaderModule, _prefixSumTurnPhaseDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), prefixSumTurnPhaseShaderModule, nullptr);

	VkShaderModule prefixSumDownShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/PrefixSumDown.spv"));
	std::tie(_prefixSumDownDescriptorSetLayout, _prefixSumDownDescriptorSets) = CreatePrefixSumDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumDownPipeline, _prefixSumDownPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), prefixSumDownShaderModule, _prefixSumDownDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), prefixSumDownShaderModule, nullptr);

	VkShaderModule prefixSumDownStateShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/PrefixSumDownState.spv"));
	std::tie(_prefixSumDownStateDescriptorSetLayout, _prefixSumDownStateDescriptorSets) = CreatePrefixSumStateDescriptors(_descriptorHelper.get());
	std::tie(_prefixSumDownStatePipeline, _prefixSumDownStatePipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), prefixSumDownStateShaderModule, _prefixSumDownStateDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), prefixSumDownStateShaderModule, nullptr);

	VkShaderModule countingSortShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/CountingSort.spv"));
	std::tie(_countingSortDescriptorSetLayout, _countingSortDescriptorSets) = CreateCountingSortDescriptors(_descriptorHelper.get());
	std::tie(_countingSortPipeline, _countingSortPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), countingSortShaderModule, _countingSortDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), countingSortShaderModule, nullptr);

	VkShaderModule updateDensityShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/UpdateDensity.spv"));
	std::tie(_densityDescriptorSetLayout, _densityDescriptorSets) = CreateDensityDescriptors(_descriptorHelper.get());
	std::tie(_densityPipeline, _densityPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), updateDensityShaderModule, _densityDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), updateDensityShaderModule, nullptr);

	VkShaderModule externalForcesShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/AccumulateExternalForces.spv"));
	std::tie(_externalForcesDescriptorSetLayout, _externalForcesDescriptorSets) = CreateExternalForcesDescriptors(_descriptorHelper.get());
	std::tie(_externalForcesPipeline, _externalForcesPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), externalForcesShaderModule, _externalForcesDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), externalForcesShaderModule, nullptr);

	VkShaderModule viscosityShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/AccumulateViscosityForce.spv"));
	std::tie(_viscosityDescriptorSetLayout, _viscosityDescriptorSets) = CreateViscosityDescriptors(_descriptorHelper.get());
	std::tie(_viscosityPipeline, _viscosityPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), viscosityShaderModule, _viscosityDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), viscosityShaderModule, nullptr);

	VkShaderModule computePressureShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/ComputePressure.spv"));
	std::tie(_computePressureDescriptorSetLayout, _computePressureDescriptorSets) = CreateComputePressureDescriptors(_descriptorHelper.get());
	std::tie(_computePressurePipeline, _computePressurePipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), computePressureShaderModule, _computePressureDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), computePressureShaderModule, nullptr);

	VkShaderModule pressureForceShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/AccumulatePressureForce.spv"));
	std::tie(_pressureForceDescriptorSetLayout, _pressureForceDescriptorSets) = CreatePressureForceDescriptors(_descriptorHelper.get());
	std::tie(_pressureForcePipeline, _pressureForcePipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), pressureForceShaderModule, _pressureForceDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), pressureForceShaderModule, nullptr);

	VkShaderModule timeIntegrationShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/TimeIntegration.spv"));
	std::tie(_timeIntegrationDescriptorSetLayout, _timeIntegrationDescriptorSets) = CreateTimeIntegrationDescriptors(_descriptorHelper.get());
	std::tie(_timeIntegrationPipeline, _timeIntegrationPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), timeIntegrationShaderModule, _timeIntegrationDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), timeIntegrationShaderModule, nullptr);

	VkShaderModule endTimeStepShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/Simulation/EndTimeStep.spv"));
	std::tie(_endTimeStepDescriptorSetLayout, _endTimeStepDescriptorSets) = CreateEndTimeStepDescriptors(_descriptorHelper.get());
	std::tie(_endTimeStepPipeline, _endTimeStepPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), endTimeStepShaderModule, _endTimeStepDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), endTimeStepShaderModule, nullptr);
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
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _hashResultBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _adjacentBucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _prefixSumStateBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _gridSetupBuffer);
	descriptorHelper->BindBuffer(1, _prefixSumStateBuffer);
	descriptorHelper->BindBuffer(2, _accumulationBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreatePrefixSumStateDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _prefixSumStateBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _prefixSumStateBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreatePrefixSumTurnPhaseDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _hashResultBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _bucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _hashResultBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _bucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _adjacentBucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _densityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _velocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _forceBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateViscosityDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _hashResultBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _bucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _adjacentBucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _velocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(9, _densityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(10, _forceBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->BindBuffer(10, _forceBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateComputePressureDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _densityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _pressureBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _hashResultBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _bucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _adjacentBucketBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _velocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(9, _densityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(10, _pressureBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(11, _forceBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _simulationParametersBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _velocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _forceBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _nextVelocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _nextPositionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> SimulationCompute::CreateEndTimeStepDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _simulationSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _gridSetupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _nextVelocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _nextPositionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _positionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _velocityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _forceBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(7, _densityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(8, _pressureBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(9, _accumulationBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
