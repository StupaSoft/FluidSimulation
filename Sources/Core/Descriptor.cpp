#include "Descriptor.h"

DescriptorResource::~DescriptorResource()
{
	vkDestroyDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), _descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _descriptorSetLayout, nullptr);
}

void DescriptorResource::BindBuffer(const std::string &shaderVariable, const Buffer &buffer)
{
	auto [binding, shaderStage] = LookUpBinding(shaderVariable);
	RecordBuffer(binding, buffer);
	RecordBufferLayout(binding, shaderStage, buffer->Size(), buffer->GetDescriptorType());
}

void DescriptorResource::BindBuffers(const std::string &shaderVariable, const std::vector<Buffer> &buffers)
{
	auto [binding, shaderStage] = LookUpBinding(shaderVariable);
	RecordBuffers(binding, buffers);
	RecordBufferLayout(binding, shaderStage, buffers[0]->Size(), buffers[0]->GetDescriptorType());
}

void DescriptorResource::BindSampler(const std::string &shaderVariable, VkSampler sampler, const Image &image)
{
	auto [binding, shaderStage] = LookUpBinding(shaderVariable);
	RecordSampler(binding, sampler, image);
	RecordSamplerLayout(binding, shaderStage);
}

VkDescriptorSetLayout DescriptorResource::GetDescriptorSetLayout()
{
	if (_descriptorSetLayout == VK_NULL_HANDLE)
	{
		CreateDescriptorSetLayout();
	}

	return _descriptorSetLayout;
}

std::vector<VkDescriptorSet> &DescriptorResource::GetDescriptorSets()
{
	if (_descriptorPool == VK_NULL_HANDLE)
	{
		CreateDescriptorPool();
	}

	if (_descriptorSets.empty())
	{
		CreateDescriptorSets();
	}

	return _descriptorSets;
}

std::tuple<uint32_t, VkShaderStageFlags> DescriptorResource::LookUpBinding(const std::string &shaderVariable)
{
	uint32_t binding = -1;
	VkShaderStageFlags shaderStage = 0;
	for (const auto &shader : _shaders)
	{
		uint32_t bindingForShader = shader->GetBindingIndex(shaderVariable);
		if (bindingForShader != -1)
		{
			binding = bindingForShader;
			shaderStage |= shader->GetShaderStage();
		}
	}

	if (binding == -1)
	{
		// Matching binding not found!
		throw std::runtime_error(std::format("Global parameter {} not found in the shader.", shaderVariable));
	}
	else
	{
		return std::make_tuple(binding, shaderStage);
	}
}

void DescriptorResource::RecordBufferLayout(uint32_t binding, VkShaderStageFlags shaderStage, uint32_t dataSize, VkDescriptorType descriptorType)
{
	BufferLayout bufferLayout
	{
		.binding = binding,
		.shaderStage = shaderStage,
		.dataSize = dataSize,
		.descriptorType = descriptorType,
	};

	_bufferLayouts.emplace_back(bufferLayout);
}

void DescriptorResource::RecordSamplerLayout(uint32_t binding, VkShaderStageFlags shaderStage)
{
	SamplerLayout samplerLayout
	{
		.binding = binding,
		.shaderStage = shaderStage
	};

	_samplerLayouts.emplace_back(samplerLayout);
	++_poolSizesMap[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE];
}

void DescriptorResource::RecordBuffer(uint32_t binding, const Buffer &buffer)
{
	// Fill all the buffers with the same one
	std::vector<Buffer> buffers(VulkanCore::Get()->GetMaxFramesInFlight());
	std::fill(buffers.begin(), buffers.end(), buffer);

	_buffersToBind[binding] = std::move(buffers);
	_poolSizesMap[buffer->GetDescriptorType()] += VulkanCore::Get()->GetMaxFramesInFlight();
}

void DescriptorResource::RecordBuffers(uint32_t binding, const std::vector<Buffer> &buffers)
{
	if (buffers.size() != VulkanCore::Get()->GetMaxFramesInFlight())
	{
		throw std::runtime_error("Buffer size differs from the max frames in flight.");
	}

	_buffersToBind[binding] = buffers;
	_poolSizesMap[buffers[0]->GetDescriptorType()] += VulkanCore::Get()->GetMaxFramesInFlight();
}

void DescriptorResource::RecordSampler(uint32_t binding, VkSampler sampler, const Image &image)
{
	_samplersToBind[binding] = std::make_tuple(sampler, image);
}

// Create a descriptor pool from which we can allocate descriptor sets
void DescriptorResource::CreateDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (auto [descriptorType, count] : _poolSizesMap)
	{
		poolSizes.push_back({ descriptorType, count * 4 });
	}

	VkDescriptorPoolCreateInfo poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = VulkanCore::Get()->GetMaxFramesInFlight(),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	if (vkCreateDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}
}

void DescriptorResource::CreateDescriptorSetLayout()
{
	// Descriptor layout specifies the types of resources that are going to be accessed by the pipeline, 
	// just like a render pass specifies the types of attachments that will be accessed.

	// Buffer object descriptor
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	for (const auto &bufferLayout : _bufferLayouts)
	{
		VkDescriptorSetLayoutBinding layoutBinding
		{
			.binding = bufferLayout.binding, // 'binding' in the shader - layout(binding = 0) uniform MVPMatrix
			.descriptorType = bufferLayout.descriptorType,
			.descriptorCount = 1, // The number of values in the array of uniform buffer objects - only uniform buffer
			.stageFlags = bufferLayout.shaderStage, // In which shader stages the descriptor is going to be referenced
			.pImmutableSamplers = nullptr
		};

		bindings.emplace_back(layoutBinding);
	}

	// Sampler descriptor
	for (const auto &samplerLayout : _samplerLayouts)
	{
		VkDescriptorSetLayoutBinding samplerLayoutBinding
		{
			.binding = samplerLayout.binding,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = samplerLayout.shaderStage,
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

	if (vkCreateDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set layout.");
	}
}

void DescriptorResource::CreateDescriptorSets()
{
	// 1. Create descriptor sets
	std::vector<VkDescriptorSetLayout> layouts(VulkanCore::Get()->GetMaxFramesInFlight(), _descriptorSetLayout); // Same descriptor set layouts for all descriptor sets
	VkDescriptorSetAllocateInfo allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool, // Descriptor pool to allocate from
		.descriptorSetCount = VulkanCore::Get()->GetMaxFramesInFlight(), // The number of descriptor sets to allocate
		.pSetLayouts = layouts.data()
	};

	_descriptorSets.resize(VulkanCore::Get()->GetMaxFramesInFlight());
	if (vkAllocateDescriptorSets(VulkanCore::Get()->GetLogicalDevice(), &allocInfo, _descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// 2. Create descriptor writes
	// The configuration of descriptors is updated using the vkUpdateDescriptorSets function, 
	// which takes an array of VkWriteDescriptorSet structs as parameter.

	// Referred images are irrelevant to frame
	std::vector<VkDescriptorImageInfo> imageInfoList;
	for (size_t i = 0; i < _samplerLayouts.size(); ++i)
	{
		const auto &samplerLayout = _samplerLayouts[i];

		VkDescriptorImageInfo imageInfo
		{
			.sampler = std::get<0>(_samplersToBind.at(samplerLayout.binding)),
			.imageView = std::get<1>(_samplersToBind.at(samplerLayout.binding))->GetImageViewHandle(), // Now a shader can access the image view
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		imageInfoList.emplace_back(imageInfo);
	}

	for (size_t frame = 0; frame < VulkanCore::Get()->GetMaxFramesInFlight(); ++frame)
	{
		// Referred buffer differs frame by frame
		std::vector<VkDescriptorBufferInfo> bufferInfoList;
		for (size_t i = 0; i < _bufferLayouts.size(); ++i)
		{
			const auto &bufferLayout = _bufferLayouts[i];

			VkDescriptorBufferInfo bufferInfo
			{
				.buffer = _buffersToBind.at(bufferLayout.binding)[frame]->GetBufferHandle(),
				.offset = 0,
				.range = bufferLayout.dataSize
			};

			bufferInfoList.emplace_back(bufferInfo);
		}

		// Aggregate descriptor writes
		std::vector<VkWriteDescriptorSet> descriptorWrites;
		for (size_t i = 0; i < _bufferLayouts.size(); ++i)
		{
			const auto &bufferLayout = _bufferLayouts[i];
			const auto &bufferInfo = bufferInfoList[i];

			VkWriteDescriptorSet descriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = _descriptorSets[frame], // Descriptor set to update
				.dstBinding = bufferLayout.binding, // Binding index of the uniform buffer; 0
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = bufferLayout.descriptorType,

				.pBufferInfo = &bufferInfo, // Array with descriptorCount structs that actually configure the descriptors
			};

			descriptorWrites.emplace_back(descriptorWrite);
		}

		for (size_t i = 0; i < _samplerLayouts.size(); ++i)
		{
			const auto &samplerLayout = _samplerLayouts[i];
			const auto &imageInfo = imageInfoList[i];

			VkWriteDescriptorSet descriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = _descriptorSets[frame], // Descriptor set to update
				.dstBinding = samplerLayout.binding, // Binding index of the texture sampler; 3
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

				.pImageInfo = &imageInfo
			};

			descriptorWrites.emplace_back(descriptorWrite);
		}
		
		vkUpdateDescriptorSets(VulkanCore::Get()->GetLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}