#pragma once

#include <vector>
#include <memory>
#include "VulkanCore.h"

struct BufferLayout
{
	uint32_t binding = 0;
	VkDeviceSize dataSize = 0;
	VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VkShaderStageFlags shaderStages = VK_SHADER_STAGE_ALL;
};

struct SamplerLayout
{
	uint32_t binding = 0;
	VkShaderStageFlags shaderStages = VK_SHADER_STAGE_ALL;
};

class DescriptorHelper
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

	uint32_t _maxSetCount = 100;

	std::vector<VkDescriptorPoolSize> _poolSizes;
	std::vector<BufferLayout> _bufferLayouts;
	std::vector<SamplerLayout> _samplerLayouts;

	std::unordered_map<uint32_t, std::vector<VkBuffer>> _bindingBuffers; // max frames in flight
	std::unordered_map<uint32_t, std::tuple<VkSampler, VkImageView>> _bindingSamplers;

	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _descriptorSets;

	bool _needPoolRecreation = true;
	bool _needLayoutRecreation = true;
	bool _needSetRecreation = true;

public:
	DescriptorHelper(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	// Preparation
	void AddDescriptorPoolSize(const VkDescriptorPoolSize &poolSize);
	void AddBufferLayout(const BufferLayout &bufferLayout);
	void AddSamplerLayout(const SamplerLayout &samplerLayout);

	// buffer[max frames in flight]
	void BindBuffer(uint32_t binding, VkBuffer buffer);
	void BindBuffers(uint32_t binding, const std::vector<VkBuffer> &buffers);
	void BindSampler(uint32_t binding, VkSampler sampler, VkImageView imageView);

	// Finally, get descriptor set layout or descriptor sets
	VkDescriptorPool GetDescriptorPool();
	VkDescriptorSetLayout GetDescriptorSetLayout();
	std::vector<VkDescriptorSet> GetDescriptorSets();

	void ClearLayouts();

private:
	VkDescriptorPool CreateDescriptorPool(uint32_t maxSetCount, const std::vector<VkDescriptorPoolSize> &poolSizes);
	VkDescriptorSetLayout CreateDescriptorSetLayout(const std::vector<BufferLayout> &bufferLayouts, const std::vector<SamplerLayout> &samplerLayouts);
	std::vector<VkDescriptorSet> CreateDescriptorSets(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, const std::vector<BufferLayout> &bufferLayouts, const std::vector<SamplerLayout> &samplerLayouts, const std::unordered_map<uint32_t, std::vector<VkBuffer>> &bindingBuffers, const std::unordered_map<uint32_t, std::tuple<VkSampler, VkImageView>> &bindingSamplers);
};

