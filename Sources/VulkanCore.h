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
#include <type_traits>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanUtility.h"
#include "Delegate.h"
#include "Camera.h"
#include "DirectionalLight.h"

class ImageResource;
using Image = std::shared_ptr<ImageResource>;

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
	std::optional<uint32_t> computeFamily;
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	bool IsComplete()
	{
		return computeFamily.has_value() && graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class VulkanCore
{
private:
	// ==================== Basic setup ====================
	GLFWwindow *_window = nullptr;
	VkInstance _instance = VK_NULL_HANDLE; // Connection between the application and the Vulkan library

	// ==================== ValidationLayer ====================
// Validation layer
	const std::vector<const char *> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" }; // Layer included in the SDK that conducts standard validation
	// Enable validation only in the debug mode
#ifdef NDEBUG
	const bool ENABLE_VALIDATION_LAYERS = false;
#else
	const bool ENABLE_VALIDATION_LAYERS = true;
#endif

	VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE; // Handle to the validation layer

	// ==================== Physical devices and queue families ====================
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

	// ==================== Logical device and queues ====================
	VkDevice _logicalDevice = VK_NULL_HANDLE;
	
	// Belongs to the graphics queue
	// Automatically cleaned up when the device is destroyed
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	VkQueue _presentQueue = VK_NULL_HANDLE;
	VkQueue _computeQueue = VK_NULL_HANDLE;

	// ==================== Window ====================
	VkSurfaceKHR _surface = VK_NULL_HANDLE; // Abstract type of surface to present rendered images to
	 
	// ==================== Swap chain ====================
	VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
	const std::vector<const char *> DEVICE_EXTENSIONS =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	// Store the swap chain images
	// Will be automatically cleaned up once the swap chain has been destroyed
	std::vector<Image> _swapChainImages;
	VkFormat _swapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D _swapChainExtent { 0, 0 };

	// ==================== Render pass ====================
	VkRenderPass _renderPass = VK_NULL_HANDLE;

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> _frameBuffers;

	// ==================== Command buffers ====================
	VkCommandPool _graphicsCommandPool = VK_NULL_HANDLE;
	VkCommandPool _computeCommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> _commandBuffers;
	std::vector<VkCommandBuffer> _computeCommandBuffers;

	// ==================== Syncronization objects ====================
	std::vector<VkSemaphore> _imageAvailableSemaphores; // Signal that an image has been aquired from the swap chain
	std::vector<VkSemaphore> _renderFinishedSemaphores; // Signal that rendering has finished and presentation can happen
	std::vector<VkSemaphore> _computeFinishedSemaphores; // Signal that compute has finished 
	std::vector<VkFence> _inFlightFences; // To make sure that only one frame is rendering at a time
	std::vector<VkFence> _computeInFlightFences; // Wait until the compute shader command has ended

	// ==================== Frames in flight ====================
	static const uint32_t MAX_FRAMES_IN_FLIGHT = 2; // Limit to 2 so that the CPU doesn't get ahead of the GPU
	uint32_t _currentFrame = 0; // To use the right objects every frame

	// ==================== Window resizing ====================
	bool _framebufferResized = false;

	// ==================== Images ====================
	Image _colorImage = nullptr;

	// ==================== Depth buffering ====================
	Image _depthImage = nullptr;

	// ==================== Camera ====================
	std::shared_ptr<Camera> _mainCamera;
	const float FOV_Y = glm::radians(45.0f);

	// ==================== Light ====================
	std::shared_ptr<DirectionalLight> _mainLight;

	// ==================== Events ====================
	Delegate<void(float, uint32_t)> _onExecuteHost;
	Delegate<void(VkCommandBuffer, uint32_t)> _onRecordComputeCommand;
	Delegate<void(VkCommandBuffer, uint32_t)> _onRecordDrawCommand;
	Delegate<void()> _onRecreateSwapChain;
	Delegate<void()> _onSubmitGraphicsQueueFinishedOneShot;

public:
	static VulkanCore *Get() 
	{
		static VulkanCore vulkanCore;
		return &vulkanCore;
	}

	VulkanCore(const VulkanCore &other) = delete;
	VulkanCore(VulkanCore &&other) = default;
	VulkanCore &operator=(const VulkanCore &other) = delete;
	VulkanCore &operator=(VulkanCore &&other) = default;
	virtual ~VulkanCore();

	void InitVulkan(GLFWwindow *window);
	void UpdateFrame(float deltaSecond);
	void Resize() { _framebufferResized = true; }

	void SetUpScene(); // Temp

	// ==================== Getters ====================
	auto *GetWindow() const { return _window; }
	auto GetInstance() const { return _instance; }
	auto GetPhysicalDevice() const { return _physicalDevice; }
	auto GetLogicalDevice() const { return _logicalDevice; }
	auto GetSurface() const { return _surface; }
	auto GetComputeFamily() const { return FindQueueFamilies(_physicalDevice, _surface).computeFamily.value(); }
	auto GetGraphicsFamily() const { return FindQueueFamilies(_physicalDevice, _surface).graphicsFamily.value(); }
	auto GetPresentFamily() const { return FindQueueFamilies(_physicalDevice, _surface).presentFamily.value(); }
	auto GetGraphicsQueue() const { return _graphicsQueue; }
	auto GetMinImageCount() const { return QuerySwapChainSupport(_physicalDevice, _surface).capabilities.minImageCount; }
	auto GetSwapChainImageCount() const { return _swapChainImages.size(); }
	auto GetRenderPass() const { return _renderPass; }
	auto GetExtent() const { return _swapChainExtent; }
	auto GetGraphicsCommandPool() const { return _graphicsCommandPool; }
	auto GetComputeCommandPool() const { return _computeCommandPool; }
	auto GetMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
	auto GetCurrentFrame() const { return _currentFrame; }

	auto &GetMainCamera() const { return _mainCamera; }
	auto &GetMainLight() const { return _mainLight; }

	auto &OnExecuteHost() { return _onExecuteHost; }
	auto &OnComputeCommand() { return _onRecordComputeCommand; }
	auto &OnDrawCommand() { return _onRecordDrawCommand; }
	auto &OnRecreateSwapChain() { return _onRecreateSwapChain; }
	auto &OnSubmitGraphicsQueueFinishedOneShot() { return _onSubmitGraphicsQueueFinishedOneShot; }

	// ==================== Utility ====================
	VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool);
	void EndSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue submitQueue);
	void WaitIdle() { vkDeviceWaitIdle(_logicalDevice); }

private:
	// ==================== Singleton ====================
	VulkanCore() = default;

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
	bool IsSuitableDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const std::vector<const char *> &deviceExtensions, bool discreteGPUOnly);

	// ==================== Logical device and queues ====================
	std::tuple<VkDevice, VkQueue, VkQueue, VkQueue> CreateLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool enableValidationLayers, const std::vector<const char *> &validationLayers, const std::vector<const char *> &deviceExtensions);
	// Find queues that support graphics commands
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const;

	// ==================== Window ====================
	VkSurfaceKHR CreateSurface(VkInstance instance, GLFWwindow *window);

	// ==================== Swap chain ====================
	std::tuple<VkSwapchainKHR, std::vector<Image>, VkFormat, VkExtent2D> CreateSwapChain(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, GLFWwindow *window);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char *> &deviceExtensions);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats, VkFormat desiredFormat, VkColorSpaceKHR desiredColorSpace);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes, VkPresentModeKHR desiredPresentMode);
	VkExtent2D ChooseSwapExtent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR &capabilities);

	// ==================== Window resizing ====================
	void RecreateSwapChain();
	void CleanUpSwapChain();

	// ==================== Render pass ====================
	VkRenderPass CreateRenderPass(VkFormat swapChainImageFormat);

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> CreateFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, const std::vector<Image> &swapChainImages, const std::vector<Image> &additionalImages);

	// ==================== Command buffers ====================
	VkCommandPool CreateCommandPool(VkDevice logicalDevice, uint32_t queueFamilyIndex);
	std::vector<VkCommandBuffer> CreateCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t maxFramesInFlight);

	void RecordComputeCommandBuffer(VkCommandBuffer computeCommandBuffer, uint32_t currentFrame);
	void RecordCommandBuffer(VkExtent2D swapChainExtent, VkRenderPass renderPass, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, uint32_t currentFrame);

	// ==================== Syncronization objects ====================
	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>, std::vector<VkFence>> CreateSyncObjects(uint32_t maxFramesInFlight);

	// ==================== Depth buffering ====================
	Image CreateDepthResources(VkExtent2D swapChainExtent);
	VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	// ==================== Multisampling ====================
	Image CreateColorResources(VkFormat swapChainImageFormat, VkExtent2D swapChainExtent);
};
