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

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanUtility.h"
#include "Delegate.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "VulkanStructs.h"

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
	std::optional<uint32_t> graphicsComputeFamily;
	std::optional<uint32_t> presentFamily;
	inline bool IsComplete()
	{
		return graphicsComputeFamily.has_value() && presentFamily.has_value();
	}
};

// ==================== Helper functions ====================
class VulkanCore : public std::enable_shared_from_this<VulkanCore>
{
private:
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
	VkQueue _presentQueue;
	VkQueue _computeQueue;

	// ==================== Window ====================
	VkSurfaceKHR _surface; // Abstract type of surface to present rendered images to
	
	// ==================== Swap chain ====================
	VkSwapchainKHR _swapChain;
	const std::vector<const char *> DEVICE_EXTENSIONS =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	// Store the swap chain images
	// Will be automatically cleaned up once the swap chain has been destroyed
	std::vector<Image> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;

	// ==================== Render pass ====================
	VkRenderPass _renderPass;

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> _frameBuffers;

	// ==================== Command buffers ====================
	VkCommandPool _commandPool;
	std::vector<VkCommandBuffer> _commandBuffers;
	std::vector<VkCommandBuffer> _computeCommandBuffers;

	// ==================== Syncronization objects ====================
	std::vector<VkSemaphore> _imageAvailableSemaphores; // Signal that an image has been aquired from the swap chain
	std::vector<VkSemaphore> _renderFinishedSemaphores; // Signal that rendering has finished and presentation can happen
	std::vector<VkSemaphore> _computeFinishedSemaphores; // Signal that compute has finished 
	std::vector<VkFence> _inFlightFences; // To make sure that only one frame is rendering at a time
	std::vector<VkFence> _computeInFlightFences; // Wait until the compute shader command has ended

	// ==================== Frames in flight ====================
	static const uint32_t MAX_FRAMES_IN_FLIGHT; // Limit to 2 so that the CPU doesn't get ahead of the GPU
	uint32_t _currentFrame = 0; // To use the right objects every frame

	// ==================== Window resizing ====================
	bool _framebufferResized = false;

	// ==================== Images ====================
	Image _colorImage;

	// ==================== Depth buffering ====================
	Image _depthImage;

	// ==================== Camera ====================
	std::unique_ptr<Camera> _mainCamera;
	const float FOV_Y = glm::radians(45.0f);

	// ==================== Light ====================
	std::unique_ptr<DirectionalLight> _mainLight;

	// ==================== Events ====================
	Delegate<void(VkCommandBuffer, uint32_t)> _onRecordComputeCommand;
	Delegate<void(VkCommandBuffer, uint32_t)> _onRecordDrawCommand;
	Delegate<void()> _onCleanUpSwapChain;
	Delegate<void()> _onCleanUpOthers;
	Delegate<void()> _onRecreateSwapChain;

public:
	explicit VulkanCore(GLFWwindow *window) : _window(window) {}
	VulkanCore(const VulkanCore &other) = delete;
	VulkanCore(VulkanCore &&other) = default;
	VulkanCore &operator=(const VulkanCore &other) = delete;
	VulkanCore &operator=(VulkanCore &&other) = default;
	virtual ~VulkanCore();

	void InitVulkan();
	void DrawFrame();
	void Resize() { _framebufferResized = true; }

	void SetUpScene(); // Temp

	// ==================== Getters ====================
	auto *GetWindow() const { return _window; }
	auto GetInstance() const { return _instance; }
	auto GetPhysicalDevice() const { return _physicalDevice; }
	auto GetLogicalDevice() const { return _logicalDevice; }
	auto GetSurface() const { return _surface; }
	auto GetPresentFamily() const { return FindQueueFamilies(_physicalDevice, _surface).presentFamily.value(); }
	auto GetGraphicsFamily() const { return FindQueueFamilies(_physicalDevice, _surface).graphicsComputeFamily.value(); }
	auto GetGraphicsQueue() const { return _graphicsQueue; }
	auto GetMinImageCount() const { return QuerySwapChainSupport(_physicalDevice, _surface).capabilities.minImageCount; }
	auto GetSwapChainImageCount() const { return _swapChainImages.size(); }
	auto GetRenderPass() const { return _renderPass; }
	auto GetExtent() const { return _swapChainExtent; }
	auto GetCommandPool() const { return _commandPool; }
	auto GetMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }

	auto &GetMainCamera() const { return _mainCamera; }
	auto &GetMainLight() const { return _mainLight; }

	auto &OnComputeCommand() { return _onRecordComputeCommand; }
	auto &OnDrawCommand() { return _onRecordDrawCommand; }
	auto &OnCleanUpSwapChain() { return _onCleanUpSwapChain; }
	auto &OnCleanUpOthers() { return _onCleanUpOthers; }
	auto &OnRecreateSwapChain() { return _onRecreateSwapChain; }

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
	VkCommandPool CreateCommandPool(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface);
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
