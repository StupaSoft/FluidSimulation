#pragma once

#include <string>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class VulkanCore;

class ModelBase
{
protected:
	std::shared_ptr<VulkanCore> _vulkanCore;

public:
	ModelBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}
	ModelBase(const ModelBase &other) = delete;
	ModelBase(ModelBase &&other) = default;
	ModelBase &operator=(const ModelBase &other) = delete;
	ModelBase &operator=(ModelBase &&other) = default;
	virtual ~ModelBase() = default;

	virtual void OnCleanUpSwapChain() {};
	virtual void OnCleanUpOthers() {};
	virtual void OnRecreateSwapChain() {};

	virtual void RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
};