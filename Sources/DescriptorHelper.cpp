#include "DescriptorHelper.h"

void DescriptorHelper::AddDescriptorPoolSize(const VkDescriptorPoolSize &poolSize)
{
	_poolSizes.push_back(poolSize);
	if (poolSize.descriptorCount > _maxSetCount)
	{
		_maxSetCount = poolSize.descriptorCount;
	}
	_needPoolRecreation = true;
}

void DescriptorHelper::AddBufferLayout(uint32_t binding, VkDeviceSize dataSize, VkDescriptorType descriptorType, VkShaderStageFlags shaderStages)
{
	BufferLayout bufferLayout
	{
		.binding = binding,
		.dataSize = dataSize,
		.descriptorType = descriptorType,
		.shaderStages = shaderStages
	};

	_bufferLayouts.emplace_back(bufferLayout);
	_needLayoutRecreation = true;
}

void DescriptorHelper::AddSamplerLayout(uint32_t binding, VkShaderStageFlags shaderStages)
{
	SamplerLayout samplerLayout
	{
		.binding = binding,
		.shaderStages = shaderStages
	};

	_samplerLayouts.emplace_back(samplerLayout);
	_needLayoutRecreation = true;
}

void DescriptorHelper::BindBuffer(uint32_t binding, Buffer buffer)
{
	std::vector<Buffer> buffers;
	for (size_t i = 0; i < _vulkanCore->GetMaxFramesInFlight(); ++i)
	{
		buffers.push_back(buffer);
	}

	_bindingBuffers[binding] = std::move(buffers);
}

void DescriptorHelper::BindBuffers(uint32_t binding, const std::vector<Buffer> &buffers)
{
	if (buffers.size() != _vulkanCore->GetMaxFramesInFlight())
	{
		throw std::runtime_error("Buffer size differs from the max frames in flight.");
	}

	_bindingBuffers[binding] = buffers;
}

void DescriptorHelper::BindSampler(uint32_t binding, VkSampler sampler, Image image)
{
	_bindingSamplers[binding] = std::make_tuple(sampler, image);
}

VkDescriptorPool DescriptorHelper::GetDescriptorPool()
{
	if (_needPoolRecreation)
	{
		_descriptorPool = CreateDescriptorPool(_maxSetCount, _poolSizes);
	}

	return _descriptorPool;
}

VkDescriptorSetLayout DescriptorHelper::GetDescriptorSetLayout()
{
	if (_needLayoutRecreation)
	{
		_descriptorSetLayout = CreateDescriptorSetLayout(_bufferLayouts, _samplerLayouts);
	}

	return _descriptorSetLayout;
}

std::vector<VkDescriptorSet> DescriptorHelper::GetDescriptorSets()
{
	if (_needPoolRecreation)
	{
		_descriptorPool = CreateDescriptorPool(_maxSetCount, _poolSizes);
	}

	if (_needLayoutRecreation)
	{
		_descriptorSetLayout = CreateDescriptorSetLayout(_bufferLayouts, _samplerLayouts);
	}

	if (_needSetRecreation)
	{
		_descriptorSets = CreateDescriptorSets(_descriptorPool, _descriptorSetLayout, _bufferLayouts, _samplerLayouts, _bindingBuffers, _bindingSamplers);
	}

	return _descriptorSets;
}

// Create a descriptor pool from which we can allocate descriptor sets
VkDescriptorPool DescriptorHelper::CreateDescriptorPool(uint32_t maxSetCount, const std::vector<VkDescriptorPoolSize> &poolSizes)
{
	VkDescriptorPoolCreateInfo poolInfo
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

	_needPoolRecreation = false;

	return descriptorPool;
}

VkDescriptorSetLayout DescriptorHelper::CreateDescriptorSetLayout(const std::vector<BufferLayout> &bufferLayouts, const std::vector<SamplerLayout> &samplerLayouts)
{
	// Descriptor layout specifies the types of resources that are going to be accessed by the pipeline, 
	// just like a render pass specifies the types of attachments that will be accessed.

	// Buffer object descriptor
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	for (const auto &bufferLayout : bufferLayouts)
	{
		VkDescriptorSetLayoutBinding layoutBinding
		{
			.binding = bufferLayout.binding, // 'binding' in the shader - layout(binding = 0) uniform MVPMatrix
			.descriptorType = bufferLayout.descriptorType,
			.descriptorCount = 1, // The number of values in the array of uniform buffer objects - only uniform buffer
			.stageFlags = bufferLayout.shaderStages, // In which shader stages the descriptor is going to be referenced
			.pImmutableSamplers = nullptr
		};

		bindings.emplace_back(layoutBinding);
	}

	// Sampler descriptor
	for (const auto &samplerLayouts : samplerLayouts)
	{
		VkDescriptorSetLayoutBinding samplerLayoutBinding
		{
			.binding = samplerLayouts.binding,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = samplerLayouts.shaderStages,
			.pImmutableSamplers = nullptr
		};

		bindings.emplace_back(samplerLayoutBinding);
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo
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

	_needLayoutRecreation = false;

	return descriptorSetLayout;
}

void DescriptorHelper::ClearLayouts()
{
	_bufferLayouts.clear();
	_samplerLayouts.clear();

	_bindingBuffers.clear();
	_bindingSamplers.clear();

	_needLayoutRecreation = true;
	_needSetRecreation = true;
}

std::vector<VkDescriptorSet> DescriptorHelper::CreateDescriptorSets(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, const std::vector<BufferLayout> &bufferLayouts, const std::vector<SamplerLayout> &samplerLayouts, const std::unordered_map<uint32_t, std::vector<Buffer>> &bindingBuffers, const std::unordered_map<uint32_t, std::tuple<VkSampler, Image>> &bindingSamplers)
{
	// 1. Create descriptor sets
	std::vector<VkDescriptorSetLayout> layouts(_vulkanCore->GetMaxFramesInFlight(), descriptorSetLayout); // Same descriptor set layouts for all descriptor sets
	VkDescriptorSetAllocateInfo allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool, // Descriptor pool to allocate from
		.descriptorSetCount = _vulkanCore->GetMaxFramesInFlight(), // The number of descriptor sets to allocate
		.pSetLayouts = layouts.data()
	};

	std::vector<VkDescriptorSet> descriptorSets(_vulkanCore->GetMaxFramesInFlight());
	if (vkAllocateDescriptorSets(_vulkanCore->GetLogicalDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// 2. Create descriptor writes
	// The configuration of descriptors is updated using the vkUpdateDescriptorSets function, 
	// which takes an array of VkWriteDescriptorSet structs as parameter.

	// Referred images are irrelevant to frame
	std::vector<VkDescriptorImageInfo> imageInfoList;
	for (size_t i = 0; i < samplerLayouts.size(); ++i)
	{
		const auto &samplerLayout = samplerLayouts[i];

		VkDescriptorImageInfo imageInfo
		{
			.sampler = std::get<0>(bindingSamplers.at(samplerLayout.binding)),
			.imageView = std::get<1>(bindingSamplers.at(samplerLayout.binding))->_imageView, // Now a shader can access the image view
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		imageInfoList.emplace_back(imageInfo);
	}

	for (size_t frame = 0; frame < _vulkanCore->GetMaxFramesInFlight(); ++frame)
	{
		// Referred buffer differs frame by frame
		std::vector<VkDescriptorBufferInfo> bufferInfoList;
		for (size_t i = 0; i < bufferLayouts.size(); ++i)
		{
			const auto &bufferLayout = bufferLayouts[i];

			VkDescriptorBufferInfo bufferInfo
			{
				.buffer = bindingBuffers.at(bufferLayout.binding)[frame]->_buffer,
				.offset = 0,
				.range = bufferLayout.dataSize
			};

			bufferInfoList.emplace_back(bufferInfo);
		}

		// Aggregate descriptor writes
		std::vector<VkWriteDescriptorSet> descriptorWrites;
		for (size_t i = 0; i < bufferLayouts.size(); ++i)
		{
			const auto &bufferLayout = bufferLayouts[i];
			const auto &bufferInfo = bufferInfoList[i];

			VkWriteDescriptorSet descriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frame], // Descriptor set to update
				.dstBinding = bufferLayout.binding, // Binding index of the uniform buffer; 0
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = bufferLayout.descriptorType,

				.pBufferInfo = &bufferInfo, // Array with descriptorCount structs that actually configure the descriptors
			};

			descriptorWrites.emplace_back(descriptorWrite);
		}

		for (size_t i = 0; i < samplerLayouts.size(); ++i)
		{
			const auto &samplerLayout = samplerLayouts[i];
			const auto &imageInfo = imageInfoList[i];

			VkWriteDescriptorSet descriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frame], // Descriptor set to update
				.dstBinding = samplerLayout.binding, // Binding index of the texture sampler; 3
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

				.pImageInfo = &imageInfo
			};

			descriptorWrites.emplace_back(descriptorWrite);
		}
		
		vkUpdateDescriptorSets(_vulkanCore->GetLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return descriptorSets;
}