#pragma once

#include <string>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanCore.h"

class ModelBase : public DelegateRegistrable<ModelBase>
{
protected:
	std::shared_ptr<VulkanCore> _vulkanCore;
	size_t _eventID = 0;

public:
	ModelBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}
	ModelBase(const ModelBase &other) = delete;
	ModelBase(ModelBase &&other) = default;
	ModelBase &operator=(const ModelBase &other) = delete;
	ModelBase &operator=(ModelBase &&other) = default;
	virtual ~ModelBase() = default;

	virtual void Register() override;

	virtual void RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame) = 0;
	virtual uint32_t GetOrder() = 0;
};