#pragma once

#include <vector>
#include <memory>
#include "VulkanCore.h"
#include "VulkanResources.h"
#include "ShaderResource.h"

class DescriptorResource;
using Descriptor = std::unique_ptr<DescriptorResource>;
inline Descriptor CreateDescriptor(const std::vector<ShaderAsset> &shaders) { return std::make_unique<DescriptorResource>(shaders); }
inline Descriptor CreateDescriptor(const ShaderAsset &shader) { return CreateDescriptor(std::vector<ShaderAsset>{ shader }); }

class DescriptorResource
{
private:
	struct BufferLayout
	{
		uint32_t binding = 0;
		VkShaderStageFlags shaderStage = 0;
		VkDeviceSize dataSize = 0;
		VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	};

	struct SamplerLayout
	{
		uint32_t binding = 0;
		VkShaderStageFlags shaderStage = 0;
	};

	std::vector<ShaderAsset> _shaders;

	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _descriptorSets;

	std::map<VkDescriptorType, uint32_t> _poolSizesMap;
	std::vector<BufferLayout> _bufferLayouts;
	std::vector<SamplerLayout> _samplerLayouts;

	std::unordered_map<uint32_t, std::vector<Buffer>> _buffersToBind; // max frames in flight
	std::unordered_map<uint32_t, std::tuple<VkSampler, Image>> _samplersToBind;

public:
	DescriptorResource(const std::vector<ShaderAsset> &shaders) : _shaders(shaders) {}
	DescriptorResource(const ShaderAsset &shader) : _shaders({ shader }) {}
	DescriptorResource(const DescriptorResource &other) = delete;
	DescriptorResource &operator=(const DescriptorResource &other) = delete;
	DescriptorResource(DescriptorResource &&other) = delete;
	DescriptorResource &operator=(DescriptorResource &&other) = delete;
	virtual ~DescriptorResource();

	// Store info (layout, buffer, sampler, ...)
	void BindBuffer(const std::string &shaderVariable, const Buffer &buffer);
	void BindBuffers(const std::string &shaderVariable, const std::vector<Buffer> &buffers);
	void BindSampler(const std::string &shaderVariable, VkSampler sampler, const Image &image);

	VkDescriptorSetLayout GetDescriptorSetLayout();
	std::vector<VkDescriptorSet> &GetDescriptorSets();

private:
	std::tuple<uint32_t, VkShaderStageFlags> LookUpBinding(const std::string &shaderVariable);

	void RecordBufferLayout(uint32_t binding, VkShaderStageFlags shaderStage, uint32_t dataSize, VkDescriptorType descriptorType);
	void RecordSamplerLayout(uint32_t binding, VkShaderStageFlags shaderStage);

	void RecordBuffer(uint32_t binding, const Buffer &buffer);
	void RecordBuffers(uint32_t binding, const std::vector<Buffer> &buffers);
	void RecordSampler(uint32_t binding, VkSampler sampler, const Image &image);

	void CreateDescriptorPool();
	void CreateDescriptorSetLayout();
	void CreateDescriptorSets();
};

