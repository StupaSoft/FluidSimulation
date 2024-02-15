#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class VulkanCore;

class ComputeBase
{
protected:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

public:
	ComputeBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}
	ComputeBase(const ComputeBase &other) = delete;
	ComputeBase(ComputeBase &&other) = default;
	ComputeBase &operator=(const ComputeBase &other) = delete;
	ComputeBase &operator=(ComputeBase &&other) = default;
	virtual ~ComputeBase() = default;

	virtual void RecordCommand(VkCommandBuffer commandBuffer, VkCommandBuffer computeCommandBuffer, uint32_t currentFrame) = 0;
	virtual uint32_t GetOrder() = 0;
};
