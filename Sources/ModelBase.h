#pragma once

#include <string>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct ModelInitInfo
{
	VkInstance _instance;
	GLFWwindow *_window;
	VkSurfaceKHR _surface;
	uint32_t _queueFamily;
	VkQueue _graphicsQueue;
	uint32_t _minImageCount;
	size_t _swapChainImageCount;
	VkPhysicalDevice _physicalDevice;
	VkDevice _logicalDevice;
	VkRenderPass _renderPass;
	VkExtent2D _swapChainExtent;
	VkCommandPool _commandPool;
	uint32_t _maxFramesInFlight;
};

class ModelBase
{
protected:
	ModelInitInfo _modelInitInfo;

public:
	ModelBase(const ModelInitInfo &modelInitInfo) : _modelInitInfo(modelInitInfo) {}

	virtual void OnCleanUpSwapChain() {};
	virtual void OnCleanUpOthers() {};
	virtual void OnRecreateSwapChain(const ModelInitInfo &modelInitInfo) {};

	virtual void RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
};