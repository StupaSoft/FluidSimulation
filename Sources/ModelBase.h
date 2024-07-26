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
	size_t _eventID = 0;
	size_t _order = PRIORITY_LOWEST;

public:
	ModelBase() = default;
	ModelBase(const ModelBase &other) = delete;
	ModelBase(ModelBase &&other) = default;
	ModelBase &operator=(const ModelBase &other) = delete;
	ModelBase &operator=(ModelBase &&other) = default;
	virtual ~ModelBase() = default;

	virtual void Register() override;

	virtual void RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame) = 0;
	size_t GetOrder() { return _order; }
	void SetOrder(size_t order) { _order = order; }
};