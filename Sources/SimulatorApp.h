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

class SimulatorApp
{
public:
	void Run();

private:
	// ==================== Base codes ====================
	VkInstance instance; // Vulkan instance

	// Window
	GLFWwindow *window;
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	// ==================== ValidationLayer ====================
	// Validation layer
	const std::vector<const char *> validationLayers = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	VkDebugUtilsMessengerEXT debugMessenger; // Validation layer handle

	// ==================== Physical devices and queue families ====================
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	// ==================== Logical device and queues ====================
	VkDevice logicalDevice;
	VkQueue graphicsQueue; // Automatically cleaned up when the device is destroyed

	// ==================== Window ====================
	VkSurfaceKHR surface; // Abstract type of surface to present rendered images to
	VkQueue presentQueue;

	// ==================== Swap chain ====================
	VkSwapchainKHR swapChain;
	const std::vector<const char *> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	// Store the swap chain images
	// Will be automatically cleaned up once the swap chain has been destroyed
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	// ==================== Image views ====================
	std::vector<VkImageView> swapChainImageViews;

	// ==================== Graphics pipeline ====================
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout; // Pipeline layout

	// ==================== Render pass ====================
	VkRenderPass renderPass;

	// ==================== Framebuffers ====================
	std::vector<VkFramebuffer> swapChainFramebuffers;

	// ==================== Command buffers ====================
	VkCommandPool commandPool; // Manages the memory where command buffers are allocated and command buffers are allocated from them
	std::vector<VkCommandBuffer> commandBuffers;

	// ==================== Syncronization objects ====================
	std::vector<VkSemaphore> imageAvailableSemaphores; // Signal that an image has been aquired from the swap chain
	std::vector<VkSemaphore> renderFinishedSemaphores; // Signal that rendering has finished and presentation can happen
	std::vector<VkFence> inFlightFences; // To make sure that only one frame is rendering at a time

	// ==================== Frames in flight ====================
	const int MAX_FRAMES_IN_FLIGHT = 2; // Limit to 2 so that the CPU doesn't get ahead of the GPU
	uint32_t currentFrame = 0; // To use the right objects every frame

	// ==================== Window resizing ====================
	bool framebufferResized = false;

	// ==================== Vertex input ====================
	std::vector<Vertex> vertices;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	// ==================== Index buffer ====================
	std::vector<uint32_t> indices;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	// ==================== Descriptor layout and buffers ====================
	VkDescriptorSetLayout descriptorSetLayout;
	std::vector<VkBuffer> uniformBuffers; // Create multiple buffers for each frame
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	// ==================== Descriptor pool and sets ====================
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	// ==================== Images ====================
	VkDeviceMemory textureImageMemory;

	// ==================== Image view and sampler ====================
	VkImageView textureImageView;
	VkSampler textureSampler;

	// ==================== Depth buffering ====================
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	// ==================== Loading models ====================
	const std::string MODEL_PATH = "Models/M2A1.obj";
	const std::string TEXTURE_PATH = "Textures/M2A1_diffuse.png";

	// ==================== Mipmaps ====================
	uint32_t textureMipLevels;
	VkImage textureImage;

	// ==================== Multisampling ====================
	VkSampleCountFlagBits msaaSampleCount = VK_SAMPLE_COUNT_1_BIT;
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;

	// ==================== Basic functions ====================
	void InitWindow();
	void InitVulkan();
	void MainLoop();
	void CleanUp();

	// ==================== Helper functions ====================
	void CreateInstance();
	bool CheckValidationLayerSupport();
	std::vector<const char *> GetRequiredExtensions();
	void SetupDebugMessenger();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo);
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator);
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

	// ==================== Physical devices and queue families ====================
	void PickPhysicalDevice();
	bool IsSuitableDevice(VkPhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	// ==================== Logical device and queues ====================
	void CreateLogicalDevice();

	// ==================== Window ====================
	void CreateSurface();

	// ==================== Swap chain ====================
	void CreateSwapChain();
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	// ==================== Image views ====================
	void CreateImageViews();

	// ==================== Graphics pipeline ====================
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<char> &code);

	// ==================== Render pass ====================
	void CreateRenderPass();

	// ==================== Framebuffers ====================
	void CreateFramebuffers();

	// ==================== Command buffers ====================
	void CreateCommandPool();
	void CreateCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	// ==================== Syncronization objects ====================
	void CreateSyncObjects();

	// ==================== Rendering and presentation ====================
	void DrawFrame();

	// ==================== Window resizing ====================
	void RecreateSwapChain();
	void CleanUpSwapChain();
	static void OnFramebufferResized(GLFWwindow *window, int width, int height);

	// ==================== Vertex input ====================
	void CreateVertexBuffer();
	uint32_t FindMemoryType(uint32_t typeFilter, const VkMemoryPropertyFlags &properties);

	// ==================== Staging buffer ====================
	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	// ==================== Index buffer ====================
	void CreateIndexBuffer();

	// ==================== Descriptor layout and buffers ====================
	void CreateDescriptorSetLayout();
	void CreateUniformBuffers();
	void UpdateUniformBuffer();

	// ==================== Descriptor pool and sets ====================
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	// ==================== Images ====================
	void CreateTextureImage();
	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *imageMemory);
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	// ==================== Image view and sampler ====================
	void CreateTextureImageView();
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void CreateTextureSampler();

	// ==================== Depth buffering ====================
	void CreateDepthResources();
	VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	// ==================== Loading a model ====================
	void LoadModel();

	// ==================== Mipmaps ====================
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// ==================== Multisampling ====================
	VkSampleCountFlagBits GetMaxUsableSampleCount();
	void CreateColorResources();
};
