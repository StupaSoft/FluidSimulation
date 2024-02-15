#pragma once

#include <string>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanCore.h"

class ModelBase
{
protected:
	std::shared_ptr<VulkanCore> _vulkanCore;
	size_t _eventID = 0;

public:
	ModelBase(const std::shared_ptr<VulkanCore> &vulkanCore);
	ModelBase(const ModelBase &other) = delete;
	ModelBase(ModelBase &&other) = default;
	ModelBase &operator=(const ModelBase &other) = delete;
	ModelBase &operator=(ModelBase &&other) = default;
	virtual ~ModelBase();

	virtual void RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
	virtual uint32_t GetOrder() = 0;
};