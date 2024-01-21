#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <string>
#include <format>
#include <optional>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
#include <array>
#include <chrono>
#include <unordered_map>

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "FileManager.h"
#include "VulkanUtility.h"
#include "ModelBase.h"

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices
{
	// Note that queue families that support drawing commands and presentation can differ.
	// So they are separated.
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	inline bool IsComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

// ==================== Helper functions ====================
class VulkanCore
{
private:
	// ==================== Model pool ====================
	std::vector<std::shared_ptr<ModelBase>> _models;

	// ==================== Basic setup ====================
	GLFWwindow *_window;

	VkInstance _instance; // Connection between the application and the Vulkan library

	// ==================== ValidationLayer ====================
// Validation layer
	const std::vector<const char *> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" }; // Layer included in the SDK that conducts standard validation
	// Enable validation only in the debug mode
#ifdef NDEBUG
	const bool ENABLE_VALIDATION_LAYERS = false;
#else
	const bool ENABLE_VALIDATION_LAYERS = true;
#endif

	VkDebugUtilsMessengerEXT _debugMessenger; // Handle to the validation layer

	// ==================== Physical devices and queue families ====================
	VkPhysicalDevice _physicalDevice;

	// ==================== Logical device and queues ====================
	VkDevice _logicalDevice;
	
	// Belongs to the graphics queue
	// Automatically cleaned up when the device is destroyed
	VkQueue _graphicsQueue;

	// ==================== Window ====================
	VkSurfaceKHR _surface; // Abstract type of surface to present rendered images to
	VkQueue _presentQueue;

	// ==================== Swap chain ====================
	VkSwapchainKHR _swapChain;
	const std::vector<const char *> DEVICE_EXTENSIONS =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	// Store the swap chain images
	// Will be automatically cleaned up once the swap chain has been destroyed
	std::vector<VkImage> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;
	std::vector<VkImageView> _swapChainImageViews;

	// ==================== Render pass ====================
	VkRenderPass _renderPass;

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> _frameBuffers;

	// ==================== Command buffers ====================
	VkCommandPool _commandPool;
	std::vector<VkCommandBuffer> _commandBuffers;

	// ==================== Syncronization objects ====================
	std::vector<VkSemaphore> _imageAvailableSemaphores; // Signal that an image has been aquired from the swap chain
	std::vector<VkSemaphore> _renderFinishedSemaphores; // Signal that rendering has finished and presentation can happen
	std::vector<VkFence> _inFlightFences; // To make sure that only one frame is rendering at a time

	// ==================== Frames in flight ====================
	static const uint32_t MAX_FRAMES_IN_FLIGHT; // Limit to 2 so that the CPU doesn't get ahead of the GPU
	uint32_t _currentFrame = 0; // To use the right objects every frame

	// ==================== Window resizing ====================
	bool _framebufferResized = false;

	// ==================== Uniform buffers ====================
	std::vector<VkBuffer> _uniformBuffers; // Create multiple buffers for each frame
	std::vector<VkDeviceMemory> _uniformBuffersMemory;

	// ==================== Images ====================
	VkImage _colorImage;
	VkDeviceMemory _colorImageMemory;
	VkImageView _colorImageView;

	// ==================== Depth buffering ====================
	VkImage _depthImage;
	VkDeviceMemory _depthImageMemory;
	VkImageView _depthImageView;

public:
	explicit VulkanCore(GLFWwindow *window) : _window(window) {}
	VulkanCore(const VulkanCore &other) = delete;
	VulkanCore(VulkanCore &&other) = default;
	VulkanCore &operator=(const VulkanCore &other) = delete;
	VulkanCore &operator=(VulkanCore &&other) = default;
	virtual ~VulkanCore();

	void InitVulkan();

	template<typename TModel, typename... TArgs>
	std::shared_ptr<TModel> AddModel(TArgs&&... args)
	{
		ModelInitInfo modelInitInfo =
		{
			._instance = _instance,
			._window = _window,
			._surface = _surface,
			._queueFamily = FindQueueFamilies(_physicalDevice, _surface).graphicsFamily.value(),
			._graphicsQueue = _graphicsQueue,
			._minImageCount = QuerySwapChainSupport(_physicalDevice, _surface).capabilities.minImageCount,
			._swapChainImageCount = _swapChainImages.size(),
			._physicalDevice = _physicalDevice,
			._logicalDevice = _logicalDevice,
			._renderPass = _renderPass,
			._swapChainExtent = _swapChainExtent,
			._commandPool = _commandPool,
			._maxFramesInFlight = MAX_FRAMES_IN_FLIGHT
		};

		std::shared_ptr<TModel> model = std::make_shared<TModel>(modelInitInfo, std::forward<TArgs>(args)...);
		_models.emplace_back(model);

		return model;
	}
	void RemoveModel(const std::shared_ptr<ModelBase> &model);

	template<typename TFunc, typename... TArgs>
	void ForAllModels(TFunc func, TArgs&&... args)
	{
		for (auto &model : _models)
		{
			((*model).*func)(std::forward<TArgs>(args)...);
		}
	}

	void DrawFrame();
	void Resize()
	{
		_framebufferResized = true;
	}

private:
	// ==================== Setup / Cleanup ====================
	VkInstance CreateInstance(bool enableValidationLayers, const std::vector<const char *> &validationLayers);
	void CleanUp();

	// ==================== Validation layer ====================
	bool CheckValidationLayerSupport(const std::vector<const char *> &validationLayers);
	std::vector<const char *> GetRequiredExtensions(bool enableValidationLayers);
	
	VkDebugUtilsMessengerEXT CreateDebugMessenger(VkInstance instance, bool enableValidationLayers);
	void SpecifyDebugLevel(VkDebugUtilsMessengerCreateInfoEXT *createInfo);
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator);
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);
	
	// ==================== Physical devices and queue families ====================
	VkPhysicalDevice SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char *> &deviceExtensions);
	bool IsSuitableDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const std::vector<const char *> &deviceExtensions);

	// ==================== Logical device and queues ====================
	std::tuple<VkDevice, VkQueue, VkQueue> CreateLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool enableValidationLayers, const std::vector<const char *> &validationLayers, const std::vector<const char *> &deviceExtensions);
	// Find queues that support graphics commands
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

	// ==================== Window ====================
	VkSurfaceKHR CreateSurface(VkInstance instance, GLFWwindow *window);

	// ==================== Swap chain ====================
	std::tuple<VkSwapchainKHR, std::vector<VkImage>, VkFormat, VkExtent2D> CreateSwapChain(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, GLFWwindow *window);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char *> &deviceExtensions);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats, VkFormat desiredFormat, VkColorSpaceKHR desiredColorSpace);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes, VkPresentModeKHR desiredPresentMode);
	VkExtent2D ChooseSwapExtent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR &capabilities);

	// ==================== Window resizing ====================
	void RecreateSwapChain();
	void CleanUpSwapChain();

	// ==================== Image views ====================
	std::vector<VkImageView> CreateImageViews(VkDevice logicalDevice, const std::vector<VkImage> &swapChainImages, VkFormat swapChainImageFormat);

	// ==================== Render pass ====================
	VkRenderPass CreateRenderPass(VkFormat swapChainImageFormat);

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> CreateFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, const std::vector<VkImageView> &swapChainImageViews, const std::vector<VkImageView> &additionalImageViews);

	// ==================== Command buffers ====================
	VkCommandPool CreateCommandPool(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface);
	std::vector<VkCommandBuffer> CreateCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t maxFramesInFlight);
	void RecordCommandBuffer(VkExtent2D swapChainExtent, VkRenderPass renderPass, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, uint32_t currentFrame);

	// ==================== Syncronization objects ====================
	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> CreateSyncObjects(uint32_t maxFramesInFlight);

	// ==================== Depth buffering ====================
	std::tuple<VkImage, VkDeviceMemory, VkImageView> CreateDepthResources(VkExtent2D swapChainExtent);
	VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	// ==================== Multisampling ====================
	std::tuple<VkImage, VkDeviceMemory, VkImageView> CreateColorResources(VkFormat swapChainImageFormat, VkExtent2D swapChainExtent);
};
