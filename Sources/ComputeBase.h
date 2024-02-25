#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanCore.h"

class ComputeBase
{
protected:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;
	size_t _eventID = 0;

public:
	ComputeBase(const std::shared_ptr<VulkanCore> &vulkanCore);
	ComputeBase(const ComputeBase &other) = delete;
	ComputeBase(ComputeBase &&other) = default;
	ComputeBase &operator=(const ComputeBase &other) = delete;
	ComputeBase &operator=(ComputeBase &&other) = default;
	virtual ~ComputeBase();

	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, uint32_t currentFrame) = 0;
	virtual uint32_t GetOrder() = 0;
	void SetEnable(bool enable);
};

