#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "VulkanCore.h"

class ComputeBase : public DelegateRegistrable
{
protected:
	
	size_t _commandRegisterID = 0;

public:
	ComputeBase() = default;
	ComputeBase(const ComputeBase &other) = delete;
	ComputeBase(ComputeBase &&other) = default;
	ComputeBase &operator=(const ComputeBase &other) = delete;
	ComputeBase &operator=(ComputeBase &&other) = default;
	virtual ~ComputeBase() = default;

	virtual void Register() override;

	void SetEnable(bool enable);

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) = 0;
};

