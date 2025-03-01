#include "MarchingCubesCompute.h"
#include "VulkanCore.h"

MarchingCubesCompute::MarchingCubesCompute(const std::vector<Buffer> &inputBuffers, size_t particleCount, const MarchingCubesGrid &marchingCubesGrid) :
	_particlePositionInputBuffers(inputBuffers)
{
	_particleProperty->_particleCount = static_cast<uint32_t>(particleCount); // Will be used for range check in the shader

	// Create and populate buffers
	CreateSetupBuffers();
	InitializationGrid(marchingCubesGrid);

	// Create descriptor pool and sets
	_descriptorHelper = std::make_shared<DescriptorHelper>();
	_descriptorPool = CreateDescriptorPool(_descriptorHelper.get());

	// Create compute pipeline
	ShaderAsset initializationShader = ShaderManager::Get()->GetShaderAsset("MarchingCubesInitialization");
	std::tie(_initializationDescriptorSetLayout, _initializationDescriptorSets) = CreateInitializationDescriptors(_descriptorHelper.get());
	std::tie(_initializationPipeline, _initializationPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), initializationShader->GetShaderModule(), _initializationDescriptorSetLayout);

	ShaderAsset accumulationShader = ShaderManager::Get()->GetShaderAsset("MarchingCubesAccumulation");
	std::tie(_accumulationDescriptorSetLayout, _accumulationDescriptorSets) = CreateAccumulationDescriptors(_descriptorHelper.get());
	std::tie(_accumulationPipeline, _accumulationPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), accumulationShader->GetShaderModule(), _accumulationDescriptorSetLayout);

	ShaderAsset constructionShader = ShaderManager::Get()->GetShaderAsset("MarchingCubesConstruction");
	std::tie(_constructionDescriptorSetLayout, _constructionDescriptorSets) = CreateConstructionDescriptors(_descriptorHelper.get());
	std::tie(_constructionPipeline, _constructionPipelineLayout) = CreateComputePipeline(VulkanCore::Get()->GetLogicalDevice(), constructionShader->GetShaderModule(), _constructionDescriptorSetLayout);
}

MarchingCubesCompute::~MarchingCubesCompute()
{
	vkDeviceWaitIdle(VulkanCore::Get()->GetLogicalDevice());

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _accumulationPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _accumulationPipelineLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _constructionPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _constructionPipelineLayout, nullptr);

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _initializationPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _initializationPipelineLayout, nullptr);

	vkDestroyDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), _descriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _accumulationDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _constructionDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _initializationDescriptorSetLayout, nullptr);
}

void MarchingCubesCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame)
{
	VkMemoryBarrier memoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
	};

	// 1. Initialization inputs and outputs
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _initializationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _initializationPipelineLayout, 0, 1, &_initializationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_setup->_voxelCount, 1024), 1, 1);

	// Synchronization - accumulation only after initialization
	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 2. Accumulate particle kernel values into voxels
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _accumulationPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _accumulationPipelineLayout, 0, 1, &_accumulationDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_particleProperty->_particleCount, 1024), 1, 1);

	// Synchronization - construction commences only after the accumulation finishes
	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	// 3. Construct meshes from the particles
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _constructionPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _constructionPipelineLayout, 0, 1, &_constructionDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_setup->_cellCount, 1024), 1, 1);
}

void MarchingCubesCompute::CreateSetupBuffers()
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_particlePropertyBuffer = CreateBuffer(sizeof(ParticleProperty), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_setupBuffer = CreateBuffer(sizeof(MarchingCubesSetup), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_indexTableBuffer = CreateBuffer(sizeof(uint32_t) * CODES_COUNT * MAX_INDICES_IN_CELL, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	memory->Bind({ _particlePropertyBuffer, _setupBuffer, _indexTableBuffer });

	_indexTableBuffer->CopyFrom(INDICES_TABLE.data());
}

void MarchingCubesCompute::CreateComputeBuffers(const MarchingCubesSetup &setup)
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_voxelBuffer = CreateBuffer(sizeof(uint32_t) * setup._voxelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_vertexBuffer = CreateBuffer(sizeof(Vertex) * setup._vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_indexBuffer = CreateBuffer(sizeof(uint32_t) * _setup->_cellCount * MAX_INDICES_IN_CELL, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); // Maximum number of indices
	_drawArgumentBuffer = CreateBuffer(sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	memory->Bind({ _voxelBuffer, _vertexBuffer, _indexBuffer, _drawArgumentBuffer });

	VkDrawIndexedIndirectCommand drawCommands
	{
		.indexCount = 0, // Will be dynamically decided in the shader
		.instanceCount = 1,
		.firstIndex = 0,
		.vertexOffset = 0,
		.firstInstance = 0
	};
	_drawArgumentBuffer->CopyFrom(&drawCommands);
}

VkDescriptorPool MarchingCubesCompute::CreateDescriptorPool(DescriptorHelper *descriptorHelper)
{
	// Create descriptor pool
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 });
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 });
	auto descriptorPool = descriptorHelper->GetDescriptorPool();

	return descriptorPool;
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubesCompute::CreateInitializationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _setupBuffer->Size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _voxelBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _drawArgumentBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _setupBuffer);
	descriptorHelper->BindBuffer(1, _voxelBuffer);
	descriptorHelper->BindBuffer(2, _drawArgumentBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorSetLayout, descriptorSets);
}

std::tuple<VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubesCompute::CreateAccumulationDescriptors(DescriptorHelper *descriptorHelper)
{
	descriptorHelper->ClearLayouts();

	// Create descriptor set layout
	descriptorHelper->AddBufferLayout(0, _particlePropertyBuffer->Size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _setupBuffer->Size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _particlePositionInputBuffers[0]->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _voxelBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
	descriptorHelper->AddBufferLayout(0, _setupBuffer->Size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, _voxelBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, _indexTableBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(3, _vertexBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(4, _indexBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(5, _drawArgumentBuffer->Size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper->BindBuffer(0, _setupBuffer);
	descriptorHelper->BindBuffer(1, _voxelBuffer);
	descriptorHelper->BindBuffer(2, _indexTableBuffer);
	descriptorHelper->BindBuffer(3, _vertexBuffer);
	descriptorHelper->BindBuffer(4, _indexBuffer);
	descriptorHelper->BindBuffer(5, _drawArgumentBuffer);
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
	_particlePropertyBuffer->CopyFrom(_particleProperty.get());
}

void MarchingCubesCompute::InitializationGrid(const MarchingCubesGrid &grid)
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

	// Create and synchronize the storage buffer
	CreateComputeBuffers(*_setup);
	_setupBuffer->CopyFrom(_setup.get());
}

void MarchingCubesCompute::SetIsovalue(float isovalue)
{
	_setup->_isovalue = isovalue;
	_setupBuffer->CopyFrom(_setup.get());
}
