#include "MarchingCubes.h"
#include "VulkanCore.h"

MarchingCubes::MarchingCubes(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const SimulationParameters &simulationParameters, const MarchingCubesSetup &setup) :
	ComputeBase(vulkanCore)
{
	_particleProperty->_particleCount = particleCount;

	// Create and populate buffers
	UpdateSetup(setup);
	UpdateParticleProperty(particleCount, simulationParameters);
	CreateComputeBuffers(*_setup);

	// Create descriptor pool and sets
	_descriptorHelper = std::make_unique<DescriptorHelper>(_vulkanCore);
	_descriptorPool = CreateDescriptorPool(_descriptorHelper.get());

	std::tie(_accumulationDescriptorSetLayout, _accumulationDescriptorSets) = CreateAccumulationDescriptors(_descriptorHelper.get());
	std::tie(_constructionDescriptorSetLayout, _constructionDescriptorSets) = CreateConstructionDescriptors(_descriptorHelper.get());
	std::tie(_presentationDescriptorSetLayout, _presentationDescriptorSets) = CreatePresentationDescriptors(_descriptorHelper.get());

	// Create compute pipeline
	VkShaderModule accumulationShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/MarchingCubesAccumulation.spv"));
	std::tie(_accumulationPipeline, _accumulationPipelineLayout) = CreateComputePipeline(accumulationShaderModule, _accumulationDescriptorSetLayout);

	VkShaderModule constructionShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/MarchingCubesConstruction.spv"));
	std::tie(_constructionPipeline, _constructionPipelineLayout) = CreateComputePipeline(constructionShaderModule, _constructionDescriptorSetLayout);

	VkShaderModule presentationShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/MarchingCubesPresentation.spv"));
	std::tie(_presentationPipeline, _presentationPipelineLayout) = CreateComputePipeline(presentationShaderModule, _presentationDescriptorSetLayout);
}

void MarchingCubes::RecordCommand(VkCommandBuffer computeCommandBuffer, uint32_t currentFrame)
{
	auto divisionCeil = [](auto x, auto y) { return (x + y - 1) / y;  };

	VkMemoryBarrier memoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
	};

	// 1. Accumulate particle kernel values into voxels
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _accumulationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _accumulationPipelineLayout, 0, 1, &_accumulationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, divisionCeil(_particleProperty->_particleCount, 1024), 1, 1); // Temp

	// Synchronization - construction commences only after the accumulation finishes
	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 2. Construct meshes from the particles
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _constructionPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _constructionPipelineLayout, 0, 1, &_constructionDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, divisionCeil(_setup->_cellCount, 1024), 1, 1);

	// Synchronization - presentation commences only after the construction finishes
	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 3. Build a presentable mesh
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _presentationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _presentationPipelineLayout, 0, 1, &_presentationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, divisionCeil(_setup->_vertexCount, 1024), 1, 1);
}

std::tuple<VkPipeline, VkPipelineLayout> MarchingCubes::CreateComputePipeline(VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout)
{
	VkPipelineShaderStageCreateInfo computeShaderStageInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = computeShaderModule,
		.pName = "main"
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout
	};

	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(_vulkanCore->GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &computePipelineLayout))
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
	if (vkCreateComputePipelines(_vulkanCore->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline))
	{
		throw std::runtime_error("Failed to create a compute pipeline.");
	}

	return std::make_tuple(computePipeline, computePipelineLayout);
}

void MarchingCubes::CreateComputeBuffers(const MarchingCubesSetup &setup)
{
	std::tie(_particlePositionBuffers, _particlePositionBufferMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * _particleProperty->_particleCount,
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_indexTableBuffer, _indexTableMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * CODES_COUNT * MAX_INDICES_IN_CELL,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	CopyToBuffer(_vulkanCore->GetLogicalDevice(), _indexTableMemory, const_cast<uint32_t *>(INDICES_TABLE.data()), 0, sizeof(uint32_t) * CODES_COUNT * MAX_INDICES_IN_CELL); // Will never change

	std::tie(_voxelBuffer, _voxelBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * setup._voxelCount,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_vertexPositionBuffer, _vertexPositionBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		sizeof(glm::vec3) * setup._vertexCount,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_normalBuffer, _normalBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * setup._vertexCount * 4,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_indexBuffer, _indexBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		sizeof(uint32_t) * setup._indexCount,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_vertexOutputBuffer, _vertexOutputBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(Vertex) * setup._vertexCount,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
}

VkDescriptorPool MarchingCubes::CreateDescriptorPool(DescriptorHelper *descriptorHelper)
{
	// Create descriptor pool
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_SET_COUNT });
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_SET_COUNT });
	auto descriptorPool = descriptorHelper->GetDescriptorPool();

	return descriptorPool;
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubes::CreateAccumulationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	BufferLayout particlePropertyLayout
	{
		.binding = 0,
		.dataSize = sizeof(ParticleProperty),
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout setupLayout
	{
		.binding = 1,
		.dataSize = sizeof(MarchingCubesSetup),
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout positionLayout
	{
		.binding = 2,
		.dataSize = sizeof(glm::vec3) * _particleProperty->_particleCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout voxelLayout
	{
		.binding = 3,
		.dataSize = sizeof(uint32_t) * _setup->_voxelCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	descriptorHelper->AddBufferLayout(particlePropertyLayout);
	descriptorHelper->AddBufferLayout(setupLayout);
	descriptorHelper->AddBufferLayout(positionLayout);
	descriptorHelper->AddBufferLayout(voxelLayout);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _particlePropertyBuffer);
	descriptorHelper->BindBuffer(1, _setupBuffer);
	descriptorHelper->BindBuffers(2, _particlePositionBuffers);
	descriptorHelper->BindBuffer(3, _voxelBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubes::CreateConstructionDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	BufferLayout setupLayout
	{
		.binding = 0,
		.dataSize = sizeof(MarchingCubesSetup),
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout voxelLayout
	{
		.binding = 1,
		.dataSize = sizeof(uint32_t) * _setup->_voxelCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout indexTableLayout
	{
		.binding = 2,
		.dataSize = sizeof(uint32_t) * CODES_COUNT * MAX_INDICES_IN_CELL,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout vertexPositionLayout
	{
		.binding = 3,
		.dataSize = sizeof(glm::vec3) * _setup->_vertexCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout normalLayout
	{
		.binding = 4,
		.dataSize = sizeof(glm::vec3) * _setup->_vertexCount * 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout indexLayout
	{
		.binding = 5,
		.dataSize = sizeof(uint32_t) * _setup->_indexCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	descriptorHelper->AddBufferLayout(setupLayout);
	descriptorHelper->AddBufferLayout(voxelLayout);
	descriptorHelper->AddBufferLayout(indexTableLayout);
	descriptorHelper->AddBufferLayout(normalLayout);
	descriptorHelper->AddBufferLayout(vertexPositionLayout);
	descriptorHelper->AddBufferLayout(indexLayout);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _setupBuffer);
	descriptorHelper->BindBuffer(1, _voxelBuffer);
	descriptorHelper->BindBuffer(2, _indexTableBuffer);
	descriptorHelper->BindBuffer(3, _vertexPositionBuffer);
	descriptorHelper->BindBuffer(4, _normalBuffer);
	descriptorHelper->BindBuffer(5, _indexBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubes::CreatePresentationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	BufferLayout setupLayout
	{
		.binding = 0,
		.dataSize = sizeof(MarchingCubesSetup),
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	// Will be reset in this stage
	BufferLayout voxelLayout
	{
		.binding = 1,
		.dataSize = sizeof(uint32_t) * _setup->_voxelCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout vertexPositionLayout
	{
		.binding = 2,
		.dataSize = sizeof(glm::vec3) * _setup->_vertexCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout normalLayout
	{
		.binding = 3,
		.dataSize = sizeof(glm::vec3) * _setup->_vertexCount * 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout vertexOutputLayout
	{
		.binding = 4,
		.dataSize = sizeof(Vertex) * _setup->_vertexCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	descriptorHelper->AddBufferLayout(setupLayout);
	descriptorHelper->AddBufferLayout(voxelLayout);
	descriptorHelper->AddBufferLayout(vertexPositionLayout);
	descriptorHelper->AddBufferLayout(normalLayout);
	descriptorHelper->AddBufferLayout(vertexOutputLayout);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _setupBuffer);
	descriptorHelper->BindBuffer(1, _voxelBuffer);
	descriptorHelper->BindBuffer(2, _vertexPositionBuffer);
	descriptorHelper->BindBuffer(3, _normalBuffer);
	descriptorHelper->BindBuffer(4, _vertexOutputBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

void MarchingCubes::UpdateParticleProperty(size_t particleCount, const SimulationParameters &simulationParameters)
{
	_particleProperty->_particleCount = particleCount;
	_particleProperty->_kernelRadiusFactor = simulationParameters._kernelRadiusFactor;

	float kernelRadius = simulationParameters._particleRadius * simulationParameters._kernelRadiusFactor;
	_particleProperty->_r1 = kernelRadius;
	_particleProperty->_r2 = kernelRadius * kernelRadius;
	_particleProperty->_r3 = kernelRadius * kernelRadius * kernelRadius;

	std::tie(_particlePropertyBuffer, _particlePropertyBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(ParticleProperty),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	CopyToBuffer(_vulkanCore->GetLogicalDevice(), _particlePropertyBufferMemory, const_cast<ParticleProperty *>(_particleProperty.get()), 0, sizeof(ParticleProperty));
}

void MarchingCubes::UpdateSetup(const MarchingCubesSetup &setup)
{
	*_setup = setup;

	uint32_t xVoxelCount = static_cast<uint32_t>((setup.xRange.y - setup.xRange.x) / setup._voxelInterval);
	uint32_t yVoxelCount = static_cast<uint32_t>((setup.yRange.y - setup.yRange.x) / setup._voxelInterval);
	uint32_t zVoxelCount = static_cast<uint32_t>((setup.zRange.y - setup.zRange.x) / setup._voxelInterval);
	_setup->_voxelDimension = glm::uvec4(xVoxelCount, yVoxelCount, zVoxelCount, 0);
	_setup->_voxelCount = xVoxelCount * yVoxelCount * zVoxelCount;

	uint32_t xCellCount = xVoxelCount - 1;
	uint32_t yCellCount = yVoxelCount - 1;
	uint32_t zCellCount = zVoxelCount - 1;
	_setup->_cellDimension = glm::uvec4(xCellCount, yCellCount, zCellCount, 0);
	_setup->_cellCount = xCellCount * yCellCount * zCellCount;

	_setup->_vertexCount = (xCellCount * (yCellCount + 1) * (zCellCount + 1)) + ((xCellCount + 1) * yCellCount * (zCellCount + 1)) + ((xCellCount + 1) * (yCellCount + 1) * zCellCount);
	_setup->_indexCount = _setup->_cellCount * MAX_INDICES_IN_CELL;

	// Synchronize with the storage buffer
	std::tie(_setupBuffer, _setupBufferMemory) = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(MarchingCubesSetup),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	CopyToBuffer(_vulkanCore->GetLogicalDevice(), _setupBufferMemory, const_cast<MarchingCubesSetup *>(_setup.get()), 0, sizeof(MarchingCubesSetup));
}

void MarchingCubes::UpdatePositions(const std::vector<glm::vec3> &positions)
{
	CopyToBuffers(_vulkanCore->GetLogicalDevice(), _particlePositionBufferMemory, const_cast<glm::vec3 *>(positions.data()), 0, sizeof(glm::vec3) * positions.size());
}

uint32_t MarchingCubes::GetOrder()
{
	return 0;
}