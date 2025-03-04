#pragma once

#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "VulkanCore.h"
#include "MathUtil.h"
#include "ShaderResource.h"

// Declarations and aliases
class PipelineAsset;
using Pipeline = std::unique_ptr<PipelineAsset>;

struct GraphicsPipelineOptions
{
	VkPrimitiveTopology _topology;
	VkPolygonMode _polygonMode;
	float _lineWidth;
};

// Container classes
class PipelineAsset
{
protected:
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
	VkPipeline _pipeline = VK_NULL_HANDLE;

public:
	virtual ~PipelineAsset();

	VkPipelineLayout GetPipelineLayout() const { return _pipelineLayout; }
	VkPipeline GetPipeline() const { return _pipeline; }

protected:
	PipelineAsset() = default;
};

class ComputePipelineAsset : public PipelineAsset
{
	friend Pipeline CreateComputePipeline(VkShaderModule comnputeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges);

protected:
	ComputePipelineAsset(VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges);
};

class GraphicsPipelineAsset : public PipelineAsset
{
	friend Pipeline CreateGraphicsPipeline(VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, VkDescriptorSetLayout descriptorSetLayout, const GraphicsPipelineOptions &options);

protected:
	GraphicsPipelineAsset(VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, VkDescriptorSetLayout descriptorSetLayout, const GraphicsPipelineOptions &options);
};

// Instantiation helper functions
inline Pipeline CreateComputePipeline(VkShaderModule comnputeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges = {})
{
	return Pipeline(new ComputePipelineAsset(comnputeShaderModule, descriptorSetLayout, pushConstantRanges));
}

inline Pipeline CreateGraphicsPipeline(VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, VkDescriptorSetLayout descriptorSetLayout, const GraphicsPipelineOptions &options)
{
	return Pipeline(new GraphicsPipelineAsset(vertShaderModule, fragShaderModule, descriptorSetLayout, options));
}





