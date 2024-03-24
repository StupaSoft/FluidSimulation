#include "MarchingCubesCompute.h"
#include "VulkanCore.h"

MarchingCubesCompute::MarchingCubesCompute(const std::shared_ptr<VulkanCore> &vulkanCore, const std::vector<Buffer> &inputBuffers, size_t particleCount, const MarchingCubesGrid &marchingCubesGrid) :
	ComputeBase(vulkanCore),
	_particlePositionInputBuffers(inputBuffers)
{
	_particleProperty->_particleCount = static_cast<uint32_t>(particleCount); // Will be used for range check in the shader

	// Create and populate buffers
	CreateSetupBuffers();
	InitializeGrid(marchingCubesGrid);

	// Create descriptor pool and sets
	_descriptorHelper = std::make_shared<DescriptorHelper>(_vulkanCore);
	_descriptorPool = CreateDescriptorPool(_descriptorHelper.get());

	// Create compute pipeline
	VkShaderModule accumulationShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/MarchingCubesAccumulation.spv"));
	std::tie(_accumulationDescriptorSetLayout, _accumulationDescriptorSets) = CreateAccumulationDescriptors(_descriptorHelper.get());
	std::tie(_accumulationPipeline, _accumulationPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), accumulationShaderModule, _accumulationDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), accumulationShaderModule, nullptr);

	VkShaderModule constructionShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/MarchingCubesConstruction.spv"));
	std::tie(_constructionDescriptorSetLayout, _constructionDescriptorSets) = CreateConstructionDescriptors(_descriptorHelper.get());
	std::tie(_constructionPipeline, _constructionPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), constructionShaderModule, _constructionDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), constructionShaderModule, nullptr);

	VkShaderModule presentationShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/MarchingCubesPresentation.spv"));
	std::tie(_presentationDescriptorSetLayout, _presentationDescriptorSets) = CreatePresentationDescriptors(_descriptorHelper.get());
	std::tie(_presentationPipeline, _presentationPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), presentationShaderModule, _presentationDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), presentationShaderModule, nullptr);
}

MarchingCubesCompute::~MarchingCubesCompute()
{
	vkDestroyPipeline(_vulkanCore->GetLogicalDevice(), _accumulationPipeline, nullptr);
	vkDestroyPipelineLayout(_vulkanCore->GetLogicalDevice(), _accumulationPipelineLayout, nullptr);

	vkDestroyPipeline(_vulkanCore->GetLogicalDevice(), _constructionPipeline, nullptr);
	vkDestroyPipelineLayout(_vulkanCore->GetLogicalDevice(), _constructionPipelineLayout, nullptr);

	vkDestroyPipeline(_vulkanCore->GetLogicalDevice(), _presentationPipeline, nullptr);
	vkDestroyPipelineLayout(_vulkanCore->GetLogicalDevice(), _presentationPipelineLayout, nullptr);

	vkDestroyDescriptorPool(_vulkanCore->GetLogicalDevice(), _descriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), _accumulationDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), _constructionDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), _presentationDescriptorSetLayout, nullptr);
}

void MarchingCubesCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame)
{
	VkMemoryBarrier memoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
	};

	// 1. Accumulate particle kernel values into voxels
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _accumulationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _accumulationPipelineLayout, 0, 1, &_accumulationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_particleProperty->_particleCount, 1024), 1, 1);

	// Synchronization - construction commences only after the accumulation finishes
	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 2. Construct meshes from the particles
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _constructionPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _constructionPipelineLayout, 0, 1, &_constructionDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_setup->_cellCount, 1024), 1, 1);

	// Synchronization - presentation commences only after the construction finishes
	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 3. Build a presentable mesh
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _presentationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _presentationPipelineLayout, 0, 1, &_presentationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_setup->_vertexCount, 1024), 1, 1);
}

void MarchingCubesCompute::CreateSetupBuffers()
{
	_particlePropertyBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(ParticleProperty),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_setupBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(MarchingCubesSetup),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_indexTableBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * CODES_COUNT * MAX_INDICES_IN_CELL,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), INDICES_TABLE.data(), _indexTableBuffer, 0); // Will never change
}

void MarchingCubesCompute::CreateComputeBuffers(const MarchingCubesSetup &setup)
{
	_voxelBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * setup._voxelCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_vertexPositionBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		sizeof(glm::vec3) * setup._vertexCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_vertexValidityBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t) * setup._vertexCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_normalBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * setup._vertexCount * 4,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_indexBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		sizeof(uint32_t) * setup._indexCount,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_vertexOutputBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(Vertex) * setup._vertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
}

VkDescriptorPool MarchingCubesCompute::CreateDescriptorPool(DescriptorHelper *descriptorHelper)
{
	// Create descriptor pool
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 });
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 });
	auto descriptorPool = descriptorHelper->GetDescriptorPool();

	return descriptorPool;
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubesCompute::CreateAccumulationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _particlePropertyBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _setupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _particlePositionInputBuffers[0]->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _voxelBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _particlePropertyBuffer);
	descriptorHelper->BindBuffer(1, _setupBuffer);
	descriptorHelper->BindBuffers(2, _particlePositionInputBuffers);
	descriptorHelper->BindBuffer(3, _voxelBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubesCompute::CreateConstructionDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _setupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _voxelBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _indexTableBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _vertexValidityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _vertexPositionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _normalBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(6, _indexBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _setupBuffer);
	descriptorHelper->BindBuffer(1, _voxelBuffer);
	descriptorHelper->BindBuffer(2, _indexTableBuffer);
	descriptorHelper->BindBuffer(3, _vertexValidityBuffer);
	descriptorHelper->BindBuffer(4, _vertexPositionBuffer);
	descriptorHelper->BindBuffer(5, _normalBuffer);
	descriptorHelper->BindBuffer(6, _indexBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubesCompute::CreatePresentationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _setupBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _voxelBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _vertexValidityBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _vertexPositionBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _normalBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _vertexOutputBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _setupBuffer);
	descriptorHelper->BindBuffer(1, _voxelBuffer);
	descriptorHelper->BindBuffer(2, _vertexValidityBuffer);
	descriptorHelper->BindBuffer(3, _vertexPositionBuffer);
	descriptorHelper->BindBuffer(4, _normalBuffer);
	descriptorHelper->BindBuffer(5, _vertexOutputBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

void MarchingCubesCompute::UpdateParticleProperty(const SimulationParameters &simulationParameters)
{
	// Particle count is not updatable once an object is instantiated.

	float kernelRadius = simulationParameters._particleRadius * simulationParameters._kernelRadiusFactor;
	_particleProperty->_r1 = kernelRadius;
	_particleProperty->_r2 = kernelRadius * kernelRadius;
	_particleProperty->_r3 = kernelRadius * kernelRadius * kernelRadius;
	
	// Synchronize with the storage buffer
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _particleProperty.get(), _particlePropertyBuffer, 0);
}

void MarchingCubesCompute::InitializeGrid(const MarchingCubesGrid &grid)
{
	_setup->_xRange = grid._xRange;
	_setup->_yRange = grid._yRange;
	_setup->_zRange = grid._zRange;
	_setup->_voxelInterval = grid._voxelInterval;

	uint32_t xVoxelCount = static_cast<uint32_t>((grid._xRange.y - grid._xRange.x) / grid._voxelInterval);
	uint32_t yVoxelCount = static_cast<uint32_t>((grid._yRange.y - grid._yRange.x) / grid._voxelInterval);
	uint32_t zVoxelCount = static_cast<uint32_t>((grid._zRange.y - grid._zRange.x) / grid._voxelInterval);
	_setup->_voxelDimension = glm::uvec4(xVoxelCount, yVoxelCount, zVoxelCount, 0);
	_setup->_voxelCount = xVoxelCount * yVoxelCount * zVoxelCount;

	uint32_t xCellCount = xVoxelCount - 1;
	uint32_t yCellCount = yVoxelCount - 1;
	uint32_t zCellCount = zVoxelCount - 1;
	_setup->_cellDimension = glm::uvec4(xCellCount, yCellCount, zCellCount, 0);
	_setup->_cellCount = xCellCount * yCellCount * zCellCount;

	_setup->_vertexCount = (xCellCount * (yCellCount + 1) * (zCellCount + 1)) + ((xCellCount + 1) * yCellCount * (zCellCount + 1)) + ((xCellCount + 1) * (yCellCount + 1) * zCellCount);
	_setup->_indexCount = _setup->_cellCount * MAX_INDICES_IN_CELL;

	// Create and synchronize the storage buffer
	CreateComputeBuffers(*_setup);
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _setup.get(), _setupBuffer, 0);
}

void MarchingCubesCompute::SetIsovalue(float isovalue)
{
	_setup->_isovalue = isovalue;
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _setup.get(), _setupBuffer, 0);
}
