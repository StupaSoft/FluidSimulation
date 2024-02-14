#include "MarchingCubes.h"
#include "VulkanCore.h"

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
	_descriptorPool = CreateDescriptorPool(MAX_SET_COUNT);
	_descriptorSetLayout = CreateDescriptorSetLayout();
	_descriptorSets = CreateDescriptorSets(_descriptorSetLayout, _setupBuffers, _positionBuffers, _vertexBuffers, _indexBuffers);

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

VkDescriptorPool MarchingCubes::CreateDescriptorPool(uint32_t maxSetCount)
{
	std::vector<VkDescriptorPoolSize> poolSizes = // Which descriptor types descriptor sets are going to contain and how many of them
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSetCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxSetCount }
	};

	VkDescriptorPoolCreateInfo poolInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(maxSetCount),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(_vulkanCore->GetLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}

	return descriptorPool;
}

VkDescriptorSetLayout MarchingCubes::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding setupLayoutBinding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};

	// Particle position input
	VkDescriptorSetLayoutBinding positionLayoutBinding
	{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutBinding inLayoutBinding
	{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutBinding indexLayoutBinding
	{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};

	std::vector<VkDescriptorSetLayoutBinding> bindings = { setupLayoutBinding, positionLayoutBinding, inLayoutBinding, indexLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set layout.");
	}

	return descriptorSetLayout;
}

std::vector<VkDescriptorSet> MarchingCubes::CreateDescriptorSets(VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkBuffer> &setupBuffers, const std::vector<VkBuffer> &positionBuffers, const std::vector<VkBuffer> &vertexBuffers, const std::vector<VkBuffer> &indexBuffers)
{
	std::vector<VkDescriptorSetLayout> layouts(_vulkanCore->GetMaxFramesInFlight(), descriptorSetLayout); // Same descriptor set layouts for all descriptor sets

	VkDescriptorSetAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool, // Descriptor pool to allocate from
		.descriptorSetCount = _vulkanCore->GetMaxFramesInFlight(), // The number of descriptor sets to allocate
		.pSetLayouts = layouts.data()
	};

	std::vector<VkDescriptorSet> descriptorSets(_vulkanCore->GetMaxFramesInFlight());
	if (vkAllocateDescriptorSets(_vulkanCore->GetLogicalDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// The configuration of descriptors is updated using the vkUpdateDescriptorSets function, 
	// which takes an array of VkWriteDescriptorSet structs as parameter.
	for (size_t i = 0; i < _vulkanCore->GetMaxFramesInFlight(); ++i)
	{
		// Specify the buffer and the region within it that contains the data for the descriptor.
		VkDescriptorBufferInfo setupInfo
		{
			.buffer = _setupBuffers[i],
			.offset = 0,
			.range = sizeof(MarchingCubesSetup)
		};

		VkDescriptorBufferInfo positionInfo
		{
			.buffer = _positionBuffers[i],
			.offset = 1,
			.range = sizeof(glm::vec3)
		};

		VkDescriptorBufferInfo vertexInfo
		{
			.buffer = _vertexBuffers[i],
			.offset = 2,
			.range = sizeof(Vertex)
		};

		VkDescriptorBufferInfo indexInfo
		{
			.buffer = _indexBuffers[i],
			.offset = 3,
			.range = sizeof(uint32_t)
		};

		std::vector<VkWriteDescriptorSet> descriptorWrites
		{
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i],
				.dstBinding = 0,
				.dstArrayElement = 0,

				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

				.pBufferInfo = &setupInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i],
				.dstBinding = 1,
				.dstArrayElement = 0,

				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

				.pBufferInfo = &positionInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i],
				.dstBinding = 2,
				.dstArrayElement = 0,

				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

				.pBufferInfo = &vertexInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i],
				.dstBinding = 3,
				.dstArrayElement = 0,

				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

				.pBufferInfo = &indexInfo,
			}
		};

		vkUpdateDescriptorSets(_vulkanCore->GetLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return descriptorSets;
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
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, _descriptorSets.data(), 0, 0);

	vkCmdDispatch(computeCommandBuffer, 64, 1, 1);
}

uint32_t MarchingCubes::GetOrder()
{
	return 0;
}
