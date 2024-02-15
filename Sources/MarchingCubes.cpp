#include "MarchingCubes.h"
#include "VulkanCore.h"

const glm::uvec3 MarchingCubes::WORK_GROUP_DIMENSION = glm::uvec3(1024, 1, 1);

MarchingCubes::MarchingCubes(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const MarchingCubesSetup &setup) :
	ComputeBase(_vulkanCore)
{
	// Create and populate buffers
	std::tie(_positionBuffers, _positionBufferMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		particleCount * sizeof(glm::vec3),
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_setupBuffers, _setupBufferMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(MarchingCubesSetup),
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	UpdateSetup(setup);

	CreateMeshBuffers(setup);

	// Create descriptor sets
	std::tie(_descriptorPool, _descriptorSetLayout, _descriptorSets) = PrepareDescriptors();

	// Create compute pipeline
	std::tie(_computePipeline, _computePipelineLayout) = CreateComputePipeline(_descriptorSetLayout);
}

std::tuple<VkPipeline, VkPipelineLayout> MarchingCubes::CreateComputePipeline(VkDescriptorSetLayout descriptorSetLayout)
{
	auto computeShaderCode = ReadFile("Shaders/MarchingCubes.spv");
	VkShaderModule computeShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), computeShaderCode);

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

void MarchingCubes::CreateMeshBuffers(const MarchingCubesSetup &setup)
{
	uint32_t xCellCount = static_cast<uint32_t>((setup.xRange.y - setup.xRange.x) / setup.voxelInterval);
	uint32_t yCellCount = static_cast<uint32_t>((setup.yRange.y - setup.yRange.x) / setup.voxelInterval);
	uint32_t zCellCount = static_cast<uint32_t>((setup.zRange.y - setup.zRange.x) / setup.voxelInterval);
	uint32_t cellCount = xCellCount * yCellCount * zCellCount;

	_vertexCount = (xCellCount + yCellCount + zCellCount + 3) * (xCellCount * yCellCount + yCellCount * zCellCount + zCellCount * xCellCount);
	_indexCount = cellCount * MAX_INDICES_IN_CELL;

	std::tie(_vertexBuffers, _vertexBufferMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		_vertexCount * sizeof(Vertex), 
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_indexBuffers, _indexBufferMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		_indexCount * sizeof(uint32_t), 
		_vulkanCore->GetMaxFramesInFlight(), 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
}

std::tuple<VkDescriptorPool, VkDescriptorSetLayout, std::vector<VkDescriptorSet>> MarchingCubes::PrepareDescriptors()
{
	DescriptorHelper descriptorHelper(_vulkanCore);

	// Create descriptor pool
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_SET_COUNT });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_SET_COUNT });
	auto descriptorPool = descriptorHelper.GetDescriptorPool();

	// Create descriptor set layout
	BufferLayout setupLayout
	{
		.binding = 0,
		.dataSize = sizeof(MarchingCubesSetup),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout positionLayout
	{
		.binding = 1,
		.dataSize = sizeof(glm::vec3),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout vertexLayout
	{
		.binding = 2,
		.dataSize = sizeof(glm::vec3),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout indexLayout
	{
		.binding = 3,
		.dataSize = sizeof(uint32_t),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	descriptorHelper.AddBufferLayout(setupLayout);
	descriptorHelper.AddBufferLayout(positionLayout);
	descriptorHelper.AddBufferLayout(vertexLayout);
	descriptorHelper.AddBufferLayout(indexLayout);
	auto descriptorSetLayout = descriptorHelper.GetDescriptorSetLayout();

	// Create descriptor sets
	descriptorHelper.BindBuffers(0, _setupBuffers);
	descriptorHelper.BindBuffers(1, _positionBuffers);
	descriptorHelper.BindBuffers(2, _vertexBuffers);
	descriptorHelper.BindBuffers(3, _indexBuffers);
	auto descriptorSets = descriptorHelper.GetDescriptorSets();

	return std::make_tuple(descriptorPool, descriptorSetLayout, descriptorSets);
}

void MarchingCubes::UpdateSetup(const MarchingCubesSetup &setup)
{
	CopyToBuffer(_vulkanCore->GetLogicalDevice(), _setupBufferMemory, const_cast<MarchingCubesSetup *>(&setup), 0, sizeof(MarchingCubesSetup));
}

void MarchingCubes::RecordCommand(VkCommandBuffer commandBuffer, VkCommandBuffer computeCommandBuffer, uint32_t currentFrame)
{
	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to begin a recording command buffer.");
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &_descriptorSets[currentFrame], 0, 0);

	vkCmdDispatch(computeCommandBuffer, 1, 1, 1); // Temp
}

uint32_t MarchingCubes::GetOrder()
{
	return 0;
}
