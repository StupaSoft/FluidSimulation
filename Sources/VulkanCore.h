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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "FileManager.h"

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

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

// Descriptor used in shaders
// Use alignas to solve alignment issues
struct UniformBufferObject
{
	alignas(16) glm::mat4 model; // mat4 is binary-compatible with the shader's one
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};

inline bool operator==(const Vertex &lhs, const Vertex &rhs)
{
	return
	(
		lhs.color == rhs.color &&
		lhs.pos == rhs.pos &&
		lhs.texCoord == rhs.texCoord
	);
}

namespace std
{
	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const &vertex) const
		{
			return
			(
				hash<glm::vec3>()(vertex.pos) ^
				((hash<glm::vec3>()(vertex.color) << 1) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1)
			);
		}
	};
}

// ==================== Helper functions ====================
class VulkanCore
{
private:
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

	// ==================== Graphics pipeline ====================
	VkPipeline _graphicsPipeline;
	VkPipelineLayout _pipelineLayout;

	// ==================== Render pass ====================
	VkRenderPass _renderPass;

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> _swapChainFramebuffers;

	// ==================== Command buffers ====================
	VkCommandPool _commandPool;
	std::vector<VkCommandBuffer> _commandBuffers;

	// ==================== Syncronization objects ====================
	std::vector<VkSemaphore> _imageAvailableSemaphores; // Signal that an image has been aquired from the swap chain
	std::vector<VkSemaphore> _renderFinishedSemaphores; // Signal that rendering has finished and presentation can happen
	std::vector<VkFence> _inFlightFences; // To make sure that only one frame is rendering at a time

	// ==================== Frames in flight ====================
	const uint32_t MAX_FRAMES_IN_FLIGHT = 2; // Limit to 2 so that the CPU doesn't get ahead of the GPU
	uint32_t _currentFrame = 0; // To use the right objects every frame

	// ==================== Window resizing ====================
	bool _framebufferResized = false;

	// ==================== Vertex input ====================
	std::vector<Vertex> _vertices;
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;

	// ==================== Index buffer ====================
	std::vector<uint32_t> _indices;
	VkBuffer _indexBuffer;
	VkDeviceMemory _indexBufferMemory;

	// ==================== Descriptor layout and buffers ====================
	VkDescriptorSetLayout _descriptorSetLayout; // Specifies the type of resources that are going to be accessed by the pipeline
	std::vector<VkBuffer> _uniformBuffers; // Create multiple buffers for each frame
	std::vector<VkDeviceMemory> _uniformBuffersMemory;

	// ==================== Descriptor pool and sets ====================
	VkDescriptorPool _descriptorPool; // Descriptor sets are allocated from the descriptor pool
	std::vector<VkDescriptorSet> _descriptorSets; // Specifies the actual buffer or image resources that will be bound to the descriptors

	// ==================== Images ====================
	VkImage _colorImage;
	VkDeviceMemory _colorImageMemory;
	VkImageView _colorImageView;

	// ==================== Image view and sampler ====================
	VkImageView _textureImageView;
	VkDeviceMemory _textureImageMemory;
	VkSampler _textureSampler;

	// ==================== Depth buffering ====================
	VkImage _depthImage;
	VkDeviceMemory _depthImageMemory;
	VkImageView _depthImageView;

	// ==================== Loading models ====================
	const std::string MODEL_PATH = "Models/M2A1.obj";
	const std::string TEXTURE_PATH = "Textures/M2A1_diffuse.png";

	// ==================== Mipmaps ====================
	uint32_t _textureMipLevels;
	VkImage _textureImage;

	// ==================== ImGui ====================
	VkDescriptorPool _ImGuiDescriptorPool;
	VkRenderPass _ImGuiRenderPass;

	VkCommandPool _ImGuiCommandPool;
	std::vector<VkCommandBuffer> _ImGuiCommandBuffers;
	std::vector<VkFramebuffer> _ImGuiFramebuffers;

public:
	VulkanCore(GLFWwindow *window) : _window(window) {}
	virtual ~VulkanCore();

	void InitVulkan();
	void InitImGui();

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
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

	// ==================== Logical device and queues ====================
	std::tuple<VkDevice, VkQueue, VkQueue> CreateLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool enableValidationLayers, const std::vector<const char *> &validationLayers, const std::vector<const char *> &deviceExtensions);

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

	// ==================== Graphics pipeline ====================
	std::tuple<VkPipeline, VkPipelineLayout> CreateGraphicsPipeline(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, VkDescriptorSetLayout descriptorSetLayout);
	VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char> &code);

	// ==================== Render pass ====================
	VkRenderPass CreateRenderPass(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkFormat swapChainImageFormat);

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> CreateFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, const std::vector<VkImageView> &swapChainImageViews, const std::vector<VkImageView> &additionalImageViews);

	// ==================== Command buffers ====================
	VkCommandPool CreateCommandPool(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface);
	std::vector<VkCommandBuffer> CreateCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t maxFramesInFlight);
	void RecordCommandBuffer
	(
		VkExtent2D swapChainExtent,
		VkRenderPass renderPass,
		VkFramebuffer framebuffer,
		VkPipelineLayout pipelineLayout,
		VkPipeline graphicsPipeline,
		VkCommandBuffer commandBuffer,
		VkDescriptorSet descriptorSet,
		VkBuffer vertexBuffer,
		VkBuffer indexBuffer,
		const std::vector<uint32_t> &indices
	);

	// ==================== Syncronization objects ====================
	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> CreateSyncObjects(VkDevice logicalDevice, uint32_t maxFramesInFlight);

	// ==================== Vertex input ====================
	std::tuple<VkBuffer, VkDeviceMemory> CreateVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::vector<Vertex> &vertices);
	uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &properties);

	// ==================== Staging buffer ====================
	std::tuple<VkBuffer, VkDeviceMemory> CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void CopyBuffer(VkDevice logicalDevice, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool, VkQueue graphicsQueue);

	// ==================== Index buffer ====================
	std::tuple<VkBuffer, VkDeviceMemory> CreateIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::vector<uint32_t> &indices);

	// ==================== Descriptor layout and buffers ====================
	VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice logicalDevice);
	std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> CreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t frameCount);

	// ==================== Descriptor pool and sets ====================
	VkDescriptorPool CreateDescriptorPool(VkDevice logicalDevice, uint32_t maxFramesInFlight);
	std::vector<VkDescriptorSet> CreateDescriptorSets(VkDevice logicalDevice, const std::vector<VkBuffer> &uniformBuffers, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, VkImageView textureImageView, VkSampler textureSampler, uint32_t maxFramesInFlight);
	void UpdateUniformBuffer();

	// ==================== Images ====================
	std::tuple<VkImage, VkDeviceMemory, VkImageView, uint32_t> CreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string &texturePath);
	std::tuple<VkImage, VkDeviceMemory> CreateImageAndMemory(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	VkCommandBuffer BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
	void EndSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue submitQueue);
	void CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	// ==================== Image view and sampler ====================
	VkImageView CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	VkSampler CreateTextureSampler(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t textureMipLevels);

	// ==================== Depth buffering ====================
	std::tuple<VkImage, VkDeviceMemory, VkImageView> CreateDepthResources(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkExtent2D swapChainExtent);
	VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	// ==================== Loading a model ====================
	std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadModel(const std::string &modelPath);

	// ==================== Mipmaps ====================
	void GenerateMipmaps(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// ==================== Multisampling ====================
	VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);
	std::tuple<VkImage, VkDeviceMemory, VkImageView> CreateColorResources(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkFormat swapChainImageFormat, VkExtent2D swapChainExtent);

	// ==================== ImGui functions ====================
	VkDescriptorPool CreateImGuiDescriptorPool(VkDevice logicalDevice, uint32_t maxFramesInFlight);
	VkRenderPass CreateImGuiRenderPass(VkDevice logicalDevice, VkFormat swapChainImageFormat);
	std::vector<VkFramebuffer> CreateImGuiFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, const std::vector<VkImageView> &swapChainImageViews);
	void UpdateImGuiCommandBuffer(uint32_t imageIndex);

	void CleanUpImGui();
	void RecreateImGuiSwapChainComponents();
};
