#include "VulkanCore.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = // At which rate to load data from memory throughout the vertices
	{
		.binding = 0, // The index of the binding in the array of bindings
		.stride = sizeof(Vertex), // The number of bytes from one entry to the next
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX // Move to the next data entry after each vertex
	};

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{}; // How to extract a vertex attribute from a chunk of vertex data originating from a binding description

	attributeDescriptions[0] =
	{
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, pos)
	};

	attributeDescriptions[1] =
	{
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, color)
	};

	attributeDescriptions[2] =
	{
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex, texCoord)
	};

	return attributeDescriptions;
}

// ======================================== Public interface ========================================
VulkanCore::~VulkanCore()
{
	CleanUpImGui();
	CleanUp();
}

void VulkanCore::InitVulkan()
{
	_instance = CreateInstance(ENABLE_VALIDATION_LAYERS, VALIDATION_LAYERS);

	_debugMessenger = CreateDebugMessenger(_instance, ENABLE_VALIDATION_LAYERS);

	_surface = CreateSurface(_instance, _window); // Must be created right after the instance
	_physicalDevice = SelectPhysicalDevice(_instance, _surface, DEVICE_EXTENSIONS);
	std::tie(_logicalDevice, _graphicsQueue, _presentQueue) = CreateLogicalDevice(_physicalDevice, _surface, ENABLE_VALIDATION_LAYERS, VALIDATION_LAYERS, DEVICE_EXTENSIONS);

	std::tie(_swapChain, _swapChainImages, _swapChainImageFormat, _swapChainExtent) = CreateSwapChain(_physicalDevice, _logicalDevice, _surface, _window);
	_swapChainImageViews = CreateImageViews(_logicalDevice, _swapChainImages, _swapChainImageFormat);

	_renderPass = CreateRenderPass(_physicalDevice, _logicalDevice, _swapChainImageFormat);
	_descriptorSetLayout = CreateDescriptorSetLayout(_logicalDevice);
	std::tie(_graphicsPipeline, _pipelineLayout) = CreateGraphicsPipeline(_physicalDevice, _logicalDevice, _renderPass, _swapChainExtent, _descriptorSetLayout);
	
	_commandPool = CreateCommandPool(_physicalDevice, _logicalDevice, _surface);

	std::tie(_colorImage, _colorImageMemory, _colorImageView) = CreateColorResources(_physicalDevice, _logicalDevice, _swapChainImageFormat, _swapChainExtent);
	std::tie(_depthImage, _depthImageMemory, _depthImageView) = CreateDepthResources(_physicalDevice, _logicalDevice, _swapChainExtent);
	_swapChainFramebuffers = CreateFramebuffers(_logicalDevice, _renderPass, _swapChainExtent, _swapChainImageViews, { _depthImageView, _colorImageView });

	std::tie(_textureImage, _textureImageMemory, _textureImageView, _textureMipLevels) = CreateTextureImage(_physicalDevice, _logicalDevice, _commandPool, _graphicsQueue, TEXTURE_PATH);
	_textureSampler = CreateTextureSampler(_physicalDevice, _logicalDevice, _textureMipLevels);

	std::tie(_vertices, _indices) = LoadModel(MODEL_PATH);

	std::tie(_vertexBuffer, _vertexBufferMemory) = CreateVertexBuffer(_physicalDevice, _logicalDevice, _commandPool, _graphicsQueue, _vertices);
	std::tie(_indexBuffer, _indexBufferMemory) = CreateIndexBuffer(_physicalDevice, _logicalDevice, _commandPool, _graphicsQueue, _indices);
	std::tie(_uniformBuffers, _uniformBuffersMemory) = CreateUniformBuffers(_physicalDevice, _logicalDevice, MAX_FRAMES_IN_FLIGHT);

	_descriptorPool = CreateDescriptorPool(_logicalDevice, MAX_FRAMES_IN_FLIGHT);
	_descriptorSets = CreateDescriptorSets(_logicalDevice, _uniformBuffers, _descriptorPool, _descriptorSetLayout, _textureImageView, _textureSampler, MAX_FRAMES_IN_FLIGHT);
	_commandBuffers = CreateCommandBuffers(_logicalDevice, _commandPool, MAX_FRAMES_IN_FLIGHT);

	std::tie(_imageAvailableSemaphores, _renderFinishedSemaphores, _inFlightFences) = CreateSyncObjects(_logicalDevice, MAX_FRAMES_IN_FLIGHT);
}

void VulkanCore::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls

	ImGui::StyleColorsDark(); // Setup Dear ImGui style

	// Get ready
	_ImGuiDescriptorPool = CreateImGuiDescriptorPool(_logicalDevice, MAX_FRAMES_IN_FLIGHT);
	_ImGuiRenderPass = CreateImGuiRenderPass(_logicalDevice, _swapChainImageFormat);

	// Initialize ImGui
	ImGui_ImplGlfw_InitForVulkan(_window, true);
	ImGui_ImplVulkan_InitInfo initInfo =
	{
		.Instance = _instance,
		.PhysicalDevice = _physicalDevice,
		.Device = _logicalDevice,
		.QueueFamily = FindQueueFamilies(_physicalDevice, _surface).graphicsFamily.value(),
		.Queue = _graphicsQueue,
		.PipelineCache = VK_NULL_HANDLE,
		.DescriptorPool = _ImGuiDescriptorPool,
		.MinImageCount = QuerySwapChainSupport(_physicalDevice, _surface).capabilities.minImageCount,
		.ImageCount = static_cast<uint32_t>(_swapChainImages.size()),
		.Allocator = nullptr,
		.CheckVkResultFn = nullptr
	};

	ImGui_ImplVulkan_Init(&initInfo, _ImGuiRenderPass);

	// Create ImGui command buffers
	_ImGuiCommandPool = CreateCommandPool(_physicalDevice, _logicalDevice, _surface);
	_ImGuiCommandBuffers = CreateCommandBuffers(_logicalDevice, _ImGuiCommandPool, MAX_FRAMES_IN_FLIGHT);
	_ImGuiFramebuffers = CreateImGuiFramebuffers(_logicalDevice, _ImGuiRenderPass, _swapChainExtent, _swapChainImageViews);
}

void VulkanCore::DrawFrame()
{
	vkWaitForFences(_logicalDevice, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX); // Wait until the previous frame has finished

	uint32_t imageIndex; // Index to the image in the swap chain
	VkResult result = vkAcquireNextImageKHR(_logicalDevice, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex); // Get an available swap chain image that has become available
	if (result == VK_ERROR_OUT_OF_DATE_KHR) // The swap chain has become incompatible with the surface; usually after resizing the window
	{
		RecreateSwapChain();
		RecreateImGuiSwapChainComponents();

		_currentFrame = 0;

		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) // Even if the result is suboptimal, proceed anyway since we have already got an image.
	{
		throw std::runtime_error("Failed to acquire a swap chain image.");
	}

	vkResetFences(_logicalDevice, 1, &_inFlightFences[_currentFrame]); // Reset the fece only if we are submitting a work

	vkResetCommandBuffer(_commandBuffers[_currentFrame], 0);
	RecordCommandBuffer(_swapChainExtent, _renderPass, _swapChainFramebuffers[imageIndex], _pipelineLayout, _graphicsPipeline, _commandBuffers[_currentFrame], _descriptorSets[_currentFrame], _vertexBuffer, _indexBuffer, _indices);

	UpdateUniformBuffer();
	UpdateImGuiCommandBuffer(imageIndex);

	// Submit the command buffer
	std::vector<VkCommandBuffer> submitCommandBuffers = { _commandBuffers[imageIndex], _ImGuiCommandBuffers[imageIndex] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Wait with writing colors to the image until it's available
	VkSubmitInfo submitInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_imageAvailableSemaphores[_currentFrame],
		.pWaitDstStageMask = waitStages,

		.commandBufferCount = 2,
		.pCommandBuffers = submitCommandBuffers.data(),

		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &_renderFinishedSemaphores[_currentFrame]
	};

	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit a draw command buffer.");
	}

	VkPresentInfoKHR presentInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,

		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_renderFinishedSemaphores[_currentFrame], // Wait on the command buffer to finish execution

		.swapchainCount = 1,
		.pSwapchains = &_swapChain,
		.pImageIndices = &imageIndex
	};

	result = vkQueuePresentKHR(_presentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized) // If the result is subotpimal, recreate the swap chain because we want the best result
	{
		RecreateSwapChain();
		RecreateImGuiSwapChainComponents();

		_currentFrame = -1; // The next frame should be 0
		_framebufferResized = false;
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present a swap chain image.");
	}

	// Proceed to the next frame
	_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	vkDeviceWaitIdle(_logicalDevice);
}

// ======================================== Internal logics ========================================
VkInstance VulkanCore::CreateInstance(bool enableValidationLayers, const std::vector<const char *> &validationLayers)
{
	// Use validation layers
	if (enableValidationLayers && !CheckValidationLayerSupport(validationLayers))
	{
		throw std::runtime_error("Validation layers were requested, but unvailable.");
	}

	// Set instance information
	VkApplicationInfo appInfo =
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Fluid Simulation",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	// Fil in a create info
	VkInstanceCreateInfo createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo
	};

	// Get a list of extensions supported
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr); // 1. Get the extension count
	std::vector<VkExtensionProperties> extensions(extensionCount); // 2. Resize the vector to store extension info to the count
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()); // 3. Get the extension info

	std::vector<std::string> extensionNames(extensionCount);
	std::transform(extensions.cbegin(), extensions.cend(), extensionNames.begin(), [](const VkExtensionProperties &vp) {return vp.extensionName; }); // 4. Extract the names of extensions

	auto glfwExtensions = GetRequiredExtensions(enableValidationLayers); // Get extensions required by glfw and the validation layer
	uint32_t glfwExtensionCount = static_cast<uint32_t>(glfwExtensions.size());

	// Check if the extensions required by glfw are found among the supported extension properties
	for (uint32_t i = 0; i < glfwExtensionCount; ++i)
	{
		const char *ge = glfwExtensions[i];

		if (std::find(extensionNames.cbegin(), extensionNames.cend(), ge) == extensionNames.cend())
		{
			throw std::runtime_error(std::format("Extension %s is not found.", ge));
		}
	}
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions.data();

	// Create a debug messenger
	// The vkCreateDebugUtilMessengerEXT requires a valid instance and vkDestroyDebugUtilsMessengerEXT must be called before the instance is destroyed.
	// This curently leaves us unable to debug any issues when creating or destroying the instance (vkCreateInstance, vkDestroyInstance).
	// The solution is to pass a pointer to a VkDebugUtilsMessengerCreateInfoEXT struct to the pNext extension field of VkInstanceCreateInfo
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo; // Placed outside the if block to be used by vkCreateInstance
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		SpecifyDebugLevel(&debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo; // Add the debug callback handler
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// Create an instance
	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an instance.");
	}

	return instance;
}

void VulkanCore::CleanUp()
{
	CleanUpSwapChain();

	vkDestroyRenderPass(_logicalDevice, _renderPass, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) // Since uniform buffers depends on the number of swap chain images
	{
		vkDestroyBuffer(_logicalDevice, _uniformBuffers[i], nullptr);
		vkFreeMemory(_logicalDevice, _uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(_logicalDevice, _descriptorPool, nullptr);

	vkDestroySampler(_logicalDevice, _textureSampler, nullptr);
	vkDestroyImageView(_logicalDevice, _textureImageView, nullptr);

	vkDestroyImage(_logicalDevice, _textureImage, nullptr);
	vkFreeMemory(_logicalDevice, _textureImageMemory, nullptr);

	vkDestroyDescriptorSetLayout(_logicalDevice, _descriptorSetLayout, nullptr);

	vkDestroyBuffer(_logicalDevice, _vertexBuffer, nullptr);
	vkFreeMemory(_logicalDevice, _vertexBufferMemory, nullptr);

	vkDestroyBuffer(_logicalDevice, _indexBuffer, nullptr);
	vkFreeMemory(_logicalDevice, _indexBufferMemory, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(_logicalDevice, _renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(_logicalDevice, _inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);
	vkDestroyDevice(_logicalDevice, nullptr);

	if (ENABLE_VALIDATION_LAYERS) DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);

	vkDestroySurfaceKHR(_instance, _surface, nullptr); // Must be destroyed before the instance
	vkDestroyInstance(_instance, nullptr);

	glfwDestroyWindow(_window);
	glfwTerminate();
}

// Check whether the specified validation layers are supported
bool VulkanCore::CheckValidationLayerSupport(const std::vector<const char *> &validationLayers)
{
	// Get available layers
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// Check if the validation layer exists among the supported layers
	for (const std::string &layerName : validationLayers)
	{
		bool layerFound = false;
		for (const auto &layerProperties : availableLayers)
		{
			if (layerProperties.layerName == layerName)
			{
				layerFound = true;
			}
		}

		if (!layerFound) return false;
	}

	return true;
}

// Get extensions required by glfw and the validation layer
std::vector<const char *> VulkanCore::GetRequiredExtensions(bool enableValidationLayers)
{
	uint32_t glfwExtensionCount = 0;
	const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Conditionally add validation layers

	return extensions;
}

// Setup debug messenger after the instance is created.
VkDebugUtilsMessengerEXT VulkanCore::CreateDebugMessenger(VkInstance instance, bool enableValidationLayers)
{
	if (!enableValidationLayers) return VK_NULL_HANDLE;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	SpecifyDebugLevel(&createInfo);

	// createInfo now should be passed to the vkCreateDebugUtilsMessengerEXT function to create a VkDebugUtilsMessengerEXT
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to set up a debug messenger.");
	}

	return debugMessenger;
}

// Specify the warning level of 
void VulkanCore::SpecifyDebugLevel(VkDebugUtilsMessengerCreateInfoEXT *createInfo)
{
	*createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = // Specifies the severity of the messages that will be printed
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = DebugCallback, // The callback function
		.pUserData = nullptr
	};
}

// Since vkCreateDebugUtilsMessengerEXT is an extension function, it is not loaded automatically.
// So look up its address manually.
VkResult VulkanCore::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// Similar to CreateDebugUtilsMessengerEXT, but destroys the handle.
void VulkanCore::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

// Debug callback for validation layers
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanCore::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

// After initializing the Vulkan library through a VkInstance, we need to look for and select a graphics card in the system that supports the features we need.
VkPhysicalDevice VulkanCore::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char *> &deviceExtensions) // Set physicalDevice to a suitable physical device
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) throw std::runtime_error("Failed to find a GPU with Vulkan support.");
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	for (const auto &device : devices)
	{
		if (IsSuitableDevice(device, surface, deviceExtensions))
		{
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Failed to find a suitable GPU.");
	}

	return physicalDevice;
}

// Check whether this physical device has a suitable family queue
bool VulkanCore::IsSuitableDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const std::vector<const char *> &deviceExtensions)
{
	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
	bool extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, deviceExtensions);

	bool isSwapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice, surface);
		isSwapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// Check whether the physical device supports anisotropic filtering
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

	return indices.IsComplete() && extensionsSupported && isSwapChainAdequate && supportedFeatures.samplerAnisotropy;
}

// Find queues that support graphics commands
QueueFamilyIndices VulkanCore::FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices{};

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data()); // Get a list of queue families supported by the physical device

	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		const auto &queueFamily = queueFamilies[i];
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i; // Check if graphics queue is supported

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport); // Check if presentation queue is supported
		if (presentSupport) indices.presentFamily = i;
	}

	return indices;
}

// Create a logical device as an interface to the physical device
std::tuple<VkDevice, VkQueue, VkQueue> VulkanCore::CreateLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool enableValidationLayers, const std::vector<const char *> &validationLayers, const std::vector<const char *> &deviceExtensions)
{
	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = queueFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
		};
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures =
	{
		.sampleRateShading = VK_TRUE, // Enable sample shading feature for the device
		.samplerAnisotropy = VK_TRUE // Enable anisotropic filtering
	};

	VkDeviceCreateInfo createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = &deviceFeatures,
	};

	// Previously, Vulkan distinguished instance and device specific validation layers.
	// Up-to-date implementations ignore enabledLayerCount and ppEnabledLayerNames fields of VkDeviceCreateInfo.
	// Anyway, it is a good idea to be explicit with the specification.
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VkDevice logicalDevice = VK_NULL_HANDLE;
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS) // Create a logical device
	{
		throw std::runtime_error("Failed to create a logical device.");
	}

	// Retrieve queue handles
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);

	VkQueue presentQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);

	return std::make_tuple(logicalDevice, graphicsQueue, presentQueue);
}

VkSurfaceKHR VulkanCore::CreateSurface(VkInstance instance, GLFWwindow *window)
{
	// glfwCreateWindowSurface creates a window surface regardless of the platform
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a window surface.");
	}

	return surface;
}

// Vulkan does not have the concepth of a default framebuffer, hence it requires an infrastructure that will own the buffers we will render to, before we visualize them on the screen.
// This infrastructure is known as swap chain and must be created explicitly.
// Ths swap chain is a queue of images that are waiting to be presented to the screen.
std::tuple<VkSwapchainKHR, std::vector<VkImage>, VkFormat, VkExtent2D> VulkanCore::CreateSwapChain(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, GLFWwindow *window)
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice, surface);

	// Choose availabilities with the given swap chain support
	// Check if we can use an SRGB color space and a suitable format
	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats, VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR); // Supported image formats
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_MAILBOX_KHR); // Represent the actual conditions for showing images to the screen
	VkExtent2D extent = ChooseSwapExtent(window, swapChainSupport.capabilities); // Resolution of the swap chain images

	// Decide how many images we would like to have in the swap chain
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // What kind of operations we will use the images in the swap chain for? - render directly to them (color attachment)!
	};

	// Specify how to handle swap chain images that will be used across multiple queue families
	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	// Drawing commands on images are submitted to the graphics queue and the images are submitted to the presentation queue
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family.
		// Which queue families ownership will be shared using the queueFamilyIndexCount and pQueueFamilyIndices.
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Images can be used across multiple queue families without an explicit ownership transfer
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Don't apply any special transform to images in the swap chain 
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Specifies if the alpha channel should be used for blending with other windows
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE; // Clip obscured pixels
	createInfo.oldSwapchain = VK_NULL_HANDLE; // In case of the swap chain is invalidated, this must be a reference to the old swap chain. Will be addressed later.

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a swap chain.");
	}

	// Retrieve images that can be drawn to from the swap chain
	// The implementation is allowed to create a swap chain with more images, so pass imageCount as an argument to get the real image count.
	std::vector<VkImage> swapChainImages;
	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

	// Store the format and extent for later use
	VkFormat swapChainImageFormat = surfaceFormat.format;
	VkExtent2D swapChainExtent = extent;

	return std::make_tuple(swapChain, swapChainImages, swapChainImageFormat, swapChainExtent);
}

// Check if all the extensions specified in deviceExtensions are supported by the physical device
bool VulkanCore::CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char *> &deviceExtensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	// Check if all the required extensions are queried
	std::set<std::string> requiredExtensions(deviceExtensions.cbegin(), deviceExtensions.cend());
	for (const auto &extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

SwapChainSupportDetails VulkanCore::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) // Return supported formats and present modes 
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities); // Query basic surface capabilities

	// Query supported surface formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	details.formats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());

	// Query supported presentation modes
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	details.presentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());

	return details;
}

// Choose the right settings for the swap chain
VkSurfaceFormatKHR VulkanCore::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats, VkFormat desiredFormat, VkColorSpaceKHR desiredColorSpace)
{
	for (const auto &availableFormat : availableFormats)
	{
		if (availableFormat.format == desiredFormat && availableFormat.colorSpace == desiredColorSpace) return availableFormat;
	}

	// Otherwise return the first one
	return availableFormats[0];
}

// Choose among present modes
VkPresentModeKHR VulkanCore::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes, VkPresentModeKHR desiredPresentMode)
{
	for (const auto &availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == desiredPresentMode)
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR; // This is guaranteed to be available
}

// Pick a resoultion
VkExtent2D VulkanCore::ChooseSwapExtent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR &capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) // Allows to differ
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

// Called in case the swap chain is invalidated when, for example, the window is resized.
// Recreate swapchain-dependent objects
void VulkanCore::RecreateSwapChain()
{
	// Handle minimization
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(_window, &width, &height);
		glfwWaitEvents();
	}

	// Function calls
	vkDeviceWaitIdle(_logicalDevice);

	CleanUpSwapChain();

	std::tie(_swapChain, _swapChainImages, _swapChainImageFormat, _swapChainExtent) = CreateSwapChain(_physicalDevice, _logicalDevice, _surface, _window);
	_swapChainImageViews = CreateImageViews(_logicalDevice, _swapChainImages, _swapChainImageFormat);
	std::tie(_graphicsPipeline, _pipelineLayout) = CreateGraphicsPipeline(_physicalDevice, _logicalDevice, _renderPass, _swapChainExtent, _descriptorSetLayout);
	std::tie(_colorImage, _colorImageMemory, _colorImageView) = CreateColorResources(_physicalDevice, _logicalDevice, _swapChainImageFormat, _swapChainExtent);
	std::tie(_depthImage, _depthImageMemory, _depthImageView) = CreateDepthResources(_physicalDevice, _logicalDevice, _swapChainExtent);
	_swapChainFramebuffers = CreateFramebuffers(_logicalDevice, _renderPass, _swapChainExtent, _swapChainImageViews, { _depthImageView, _colorImageView });
}

void VulkanCore::CleanUpSwapChain()
{
	vkDestroyImageView(_logicalDevice, _depthImageView, nullptr);
	vkDestroyImage(_logicalDevice, _depthImage, nullptr);
	vkFreeMemory(_logicalDevice, _depthImageMemory, nullptr);

	vkDestroyImageView(_logicalDevice, _colorImageView, nullptr);
	vkDestroyImage(_logicalDevice, _colorImage, nullptr);
	vkFreeMemory(_logicalDevice, _colorImageMemory, nullptr);

	for (auto &framebuffer : _swapChainFramebuffers)
	{
		vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(_logicalDevice, _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);

	for (auto &imageView : _swapChainImageViews) // Explicitly destroy image views, unlike images
	{
		vkDestroyImageView(_logicalDevice, imageView, nullptr);
	}

	vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
}

std::vector<VkImageView> VulkanCore::CreateImageViews(VkDevice logicalDevice, const std::vector<VkImage> &swapChainImages, VkFormat swapChainImageFormat)
{
	std::vector<VkImageView> swapChainImageViews(swapChainImages.size()); // Resize the vector to fit all of the image views
	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		swapChainImageViews[i] = CreateImageView(logicalDevice, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	return swapChainImageViews;
}

std::tuple<VkPipeline, VkPipelineLayout> VulkanCore::CreateGraphicsPipeline(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, VkDescriptorSetLayout descriptorSetLayout)
{
	auto vertShaderCode = ReadFile("Shaders/Vert.spv");
	auto fragShaderCode = ReadFile("Shaders/Frag.spv");

	// Create shader modules
	VkShaderModule vertShaderModule = CreateShaderModule(logicalDevice, vertShaderCode);
	VkShaderModule fragShaderModule = CreateShaderModule(logicalDevice, fragShaderCode);

	// Assign shaders to a specific pipeline stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT, // Shader stage
		.module = vertShaderModule,
		.pName = "main" // Entry point
	};

	VkPipelineShaderStageCreateInfo fragShaderStageInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT, // Shader stage
		.module = fragShaderModule,
		.pName = "main" // Entry point
	};

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto vertexBindingDescription = Vertex::GetBindingDescription();
	auto vertexAttributeDescriptions = Vertex::GetAttributeDescriptions();

	// Configure vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
		.pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
	};

	// Configure an input assembly
	// Describles what kind of geometry will be drawn from the vertices and if primitive restart should be enabled
	VkPipelineInputAssemblyStateCreateInfo inputAssembly =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // Triangle from every three vertices without reuse
		.primitiveRestartEnable = VK_FALSE
	};

	// 'Resize' the image
	VkViewport viewport =
	{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)swapChainExtent.width, // Can differ from the extent of the window
		.height = (float)swapChainExtent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	// 'Crop' the image
	VkRect2D scissor =
	{
		.offset = {0, 0},
		.extent = swapChainExtent
	};

	VkPipelineViewportStateCreateInfo viewportState =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	// Configure a rasterizer that turns geometry formed by the vertex shader into fragments to be colored by the fragment shader
	VkPipelineRasterizationStateCreateInfo rasterizer =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // Whether to clamp planes out of range to near and far clipping planes
		.rasterizerDiscardEnable = VK_FALSE, // When set to true, geometry never passes through the rasterization stage
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // Since we flipped the projection
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	// Configure multisampling
	VkPipelineMultisampleStateCreateInfo multisampling =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = GetMaxUsableSampleCount(physicalDevice),
		.sampleShadingEnable = VK_TRUE, // Enable sample shading in the pipeline
		.minSampleShading = 1.0f, // Minimum fraction for sample shading; closer to one is smoother
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	// After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer.
	// Per-framebuffer configuration
	VkPipelineColorBlendAttachmentState colorBlendAttachment =
	{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	// Global color blending settings 
	VkPipelineColorBlendStateCreateInfo colorBlending =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE, // Whether to use logic operation to blend colors, instead of blendEnable
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
	};

	// Enable depth test
	VkPipelineDepthStencilStateCreateInfo depthStencil =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE, // Specifies if the depth of new fragments should be compared to the depth buffer to see if they should be discarded
		.depthWriteEnable = VK_TRUE, // Specifies if the new depth of fragments that pass the depth test should actually be written to the depth buffer
		.depthCompareOp = VK_COMPARE_OP_LESS, // The depth of new fragments should be less to pass the test.
		.depthBoundsTestEnable = VK_FALSE, // Optional depth bound test - if enabled, keep only fragments that fall within the specified depth range
		.stencilTestEnable = VK_FALSE
	};

	// Dynamic state that makes it possible to modify values without recreating the pipeline
	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
	VkPipelineDynamicStateCreateInfo dynamicState =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// Pipeline layout
	// Specify uniform values
	VkPipelineLayoutCreateInfo pipelineLayoutInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr, // A way of passing dynamic values to shaders
	};

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a pipeline layout.");
	}

	// Finally create a complete graphics pipeline with the following components
	// 1. Shader stages
	// 2. Fixed-function state
	// 3. Pipeline layout
	// 4. Render pass
	VkGraphicsPipelineCreateInfo pipelineInfo =
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,

		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = nullptr,

		.layout = pipelineLayout,

		// Any graphics pipeline is bound to a specific subpass of a render pass
		.renderPass = renderPass,
		.subpass = 0,

		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a graphics pipeline.");
	}

	// Destroy shader modules as they are already embedded into the graphics pipeline and no longer needed.
	vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);

	return std::make_tuple(graphicsPipeline, pipelineLayout);
}

VkShaderModule VulkanCore::CreateShaderModule(VkDevice logicalDevice, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t *>(code.data())
	};

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a shader module.");
	}

	return shaderModule;
}

VkRenderPass VulkanCore::CreateRenderPass(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkFormat swapChainImageFormat)
{
	// Before creating the pipeline, we need to tell Vulkan about the framebuffer attachments that will be used while rendering
	// Specify how many color and depth buffers there will be, how many samples to use for each them...

	// Create a depth buffer attachment
	VkAttachmentDescription depthAttachment =
	{
		.format = FindSupportedFormat(physicalDevice, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT), // Should be the same as the depth image itself
		.samples = GetMaxUsableSampleCount(physicalDevice),
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Don't care about storing the depth data, because it will not be used after drawing has finished.
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // Don't care about previous depth contents
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference depthAttachmentRef =
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	// Create a single color buffer attachment for MSAA
	// The image layout needs be transitioned to specific layouts that are suitable for the operation that they're going to be involved in the next.
	VkAttachmentDescription colorAttachment =
	{
		.format = swapChainImageFormat,
		.samples = GetMaxUsableSampleCount(physicalDevice),
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear the values to a constant before rendering
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE, // After the rendering, rendered contents will be stored in memory and be read later - since we are interested in presenting the image on the screen, specify as this one.
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // Which layout the image will have before the rendering pass begins - doesn't matter here, so specify it as 'undefined'. 
		// Which layout the image will automatically transition to when the render pass finishes?
		// Multisampled image cannot be presented directly.
		// Must be resolved to a regular image before presentation.
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	// A single render pass can consist of multiple subpasses.
	// Every subpass references one or more of the attachments.
	// A VkAttachmentReference refers to one of those attachments.
	VkAttachmentReference colorAttachmentRef =
	{
		// Specifies which attachment to reference by its index in the 'attachment' array below.
		// Index of the color attachment (referenced from the fragment shader with the 'layout(location = 0) out vec4 outColor' directive)
		.attachment = 1,
		// Which layout we would like the attachment to have during a subpass that uses this reference?
		// Optimal for the attachment to function as a color buffer
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
	};

	// MSAA resolve image
	VkAttachmentDescription colorAttachmentResolve =
	{
		.format = swapChainImageFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		// This one is not presenting anymore.
		// So, change the final layout to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL (ImGui)
		// .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkAttachmentReference colorAttachmentResolveRef =
	{
		.attachment = 2,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass =
	{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, // Graphics subpass, not a compute subpass
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		// Attachments used for multisampling color attachments
		// Define a multisample resolve operation which will let us render the image to the screen
		.pResolveAttachments = &colorAttachmentResolveRef, 
		.pDepthStencilAttachment = &depthAttachmentRef
	};

	// Wait for the geometry to be drawn on the framebuffer before the GUI
	VkSubpassDependency dependency =
	{
		.srcSubpass = VK_SUBPASS_EXTERNAL, // Implicit subpass before the render pass
		.dstSubpass = 0, // Our first and the only subpass

		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Stages that are being waited for (drawing geometry)
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Stages that are waiting (drawing GUI)

		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // Operations to wait on
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT // Operations that are waiting - drawing GUI
	};

	// Create the render pass itself with the attachments and the subpass
	std::array<VkAttachmentDescription, 3> attachments = { depthAttachment, colorAttachment, colorAttachmentResolve };
	VkRenderPassCreateInfo renderPassInfo =
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	VkRenderPass renderPass = VK_NULL_HANDLE;
	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a render pass.");
	}

	return renderPass;
}

// A framebuffer object references all of the VkImageView objects that represent the attachments
std::vector<VkFramebuffer> VulkanCore::CreateFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, const std::vector<VkImageView> &swapChainImageViews, const std::vector<VkImageView> &additionalImageViews)
{
	std::vector<VkFramebuffer> swapChainFramebuffers(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); ++i)
	{
		// In CreateRenderPass - std::array<VkAttachmentDescription, 3> attachments = { depthAttachment, colorAttachment, colorAttachmentResolve };
		// Provide corresponding attachements
		// { _depthImageView, _colorImageView, _swapChainImageViews[i] }
		std::vector<VkImageView> attachments = additionalImageViews;
		attachments.push_back(swapChainImageViews[i]);

		VkFramebufferCreateInfo framebufferInfo =
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(), // These VkImageView objects are bound to the respective attachment descriptions in the render pass pAttachment array.
			.width = swapChainExtent.width,
			.height = swapChainExtent.height,
			.layers = 1
		};

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a framebuffer.");
		}
	}

	return swapChainFramebuffers;
}

VkCommandPool VulkanCore::CreateCommandPool(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface)
{
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice, surface);

	VkCommandPoolCreateInfo poolInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value() // Since we want to record commands for drawing
	};

	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a command pool.");
	}

	return commandPool;
}

std::vector<VkCommandBuffer> VulkanCore::CreateCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t maxFramesInFlight)
{
	std::vector<VkCommandBuffer> commandBuffers(maxFramesInFlight);
	VkCommandBufferAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, // Can be submitted to a queue for execution, but cannot be called from other command buffers.
		.commandBufferCount = (uint32_t)commandBuffers.size()
	};

	if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers.");
	}

	return commandBuffers;
}

void VulkanCore::RecordCommandBuffer
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
)
{
	// Begin to record a command buffer
	VkCommandBufferBeginInfo beginInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to begin a recording command buffer.");
	}

	// Begin the render pass
	VkRenderPassBeginInfo renderPassBeginInfo =
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderPass, // The render pass itself
		.framebuffer = framebuffer, // We need to bind the framebuffer for the swapchain image we want to draw to.
		.renderArea =  // The size of the render area
		{
			.offset = {0, 0},
			.extent = swapChainExtent // This should match the size of attachments for best performance.
		}
	};

	// Specify clear values
	// Must be identical to the order of the attachments
	std::vector<VkClearValue> clearValues =
	{
		{
			.depthStencil = { 1.0f, 0 } // The initial value must be the farthest depth 1.0.
		},
		{
			.color = { { 0.5f, 0.5f, 0.5f, 1.0f } }
		}
	}; 
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Basic drawing commands
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // Bind vertex buffers to bindings
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32); // Bind index buffers to bindings
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	// Finishing up
	vkCmdEndRenderPass(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to record command a buffer.");
	}
}

std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> VulkanCore::CreateSyncObjects(VkDevice logicalDevice, uint32_t maxFramesInFlight)
{
	// Semaphores are used to add order between queue operations.
	// enqueue A, signal S when done - starts executing immediately
	// vkQueueSubmit(work: A, signal : S, wait : None)
	// enqueue B, wait on S to start
	// vkQueueSubmit(work: B, signal : None, wait : S)
	// Note that only the GPU waits; vkQueueSubmit returns immediately and the CPU executes continuosly.
	std::vector<VkSemaphore> imageAvailableSemaphores(maxFramesInFlight);
	std::vector<VkSemaphore> renderFinishedSemaphores(maxFramesInFlight);

	// Fences are used for ordering the execution on the CPU, otherwise known as the host.
	// enqueue A, start work immediately, signal F when done
	// vkQueueSubmit(work: A, fence : F)
	// vkWaitForFence(F)
	std::vector<VkFence> inFlightFences(maxFramesInFlight);

	VkSemaphoreCreateInfo semaphoreInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VkFenceCreateInfo fenceInfo =
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT // Create a fence in a signaled state so that we can avoid the initial blocking 
	};

	for (size_t i = 0; i < maxFramesInFlight; ++i)
	{
		bool failed =
			vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS;
		if (failed)
		{
			throw std::runtime_error("Failed to create semaphores.");
		}
	}

	return std::make_tuple(imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences);
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanCore::CreateVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::vector<Vertex> &vertices)
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// Temporary, host-visible buffer that resides on the CPU
	auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map the buffer memory into the CPU accessible memory
	void *data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize); // Copy vertex data to the mapped memory
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	auto [vertexBuffer, vertexBufferMemory] = CreateBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(logicalDevice, stagingBuffer, vertexBuffer, bufferSize, commandPool, graphicsQueue);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	return std::make_tuple(vertexBuffer, vertexBufferMemory);
}

// Find the relevant type of memory
uint32_t VulkanCore::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties); // Query info about the available types of memory

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) // Check if the memory satisfies the condition
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find a suitable memory type.");
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanCore::CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties)
{
	VkBufferCreateInfo bufferInfo =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize, // The size of the buffer in bytes
		.usage = bufferUsage, // The purpose of the buffer
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkBuffer buffer = VK_NULL_HANDLE;
	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a buffer.");
	}
	// Memory bound to the buffer has not been assigned to the buffer until here

	// Assign memory
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements); // Query memory requirements

	VkMemoryAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT - Create a memory heap that is host-coherent.
		.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};

	VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate buffer memory.");
	}

	// Associate this memory with the buffer
	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);

	return std::make_tuple(buffer, bufferMemory);
}

void VulkanCore::CopyBuffer(VkDevice logicalDevice, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool, VkQueue graphicsQueue)
{
	// Memory transfer operations are executed using command buffers.
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);

	// Now add a copy command
	VkBufferCopy copyRegion =
	{
		.size = size
	};
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanCore::CreateIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::vector<uint32_t> &indices)
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	// Allocate separate buffer, one on the CPU and the other on the GPU.
	// Temporary, host-visible buffer that resides on the CPU
	auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map the buffer memory into CPU accessible memory
	void *data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize); // Copy index data to the mapped memory
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	auto [indexBuffer, indexBufferMemory] = CreateBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(logicalDevice, stagingBuffer, indexBuffer, bufferSize, commandPool, graphicsQueue);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	return std::make_tuple(indexBuffer, indexBufferMemory);
}

VkDescriptorSetLayout VulkanCore::CreateDescriptorSetLayout(VkDevice logicalDevice)
{
	// Descriptor layout specifies the types of resources that are going to be accessed by the pipeline, 
	// just like a render pass specifies the types of attachments that will be accessed.

	// Buffer object descriptor
	VkDescriptorSetLayoutBinding uboLayoutBinding =
	{
		.binding = 0, // 'binding' in the shader - layout(binding = 0) uniform UniformBufferObject
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1, // The number of values in the array of uniform buffer objects - only uniform buffer
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // In which shader stages the descriptor is going to be referenced
		.pImmutableSamplers = nullptr
	};

	// Combined texture sampler descriptor
	VkDescriptorSetLayoutBinding samplerLayoutBinding =
	{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr
	};

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set layout.");
	}

	return descriptorSetLayout;
}

std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> VulkanCore::CreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t maxFramesInFlight)
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	std::vector<VkBuffer> uniformBuffers(maxFramesInFlight);
	std::vector<VkDeviceMemory> uniformBuffersMemory(maxFramesInFlight);

	for (size_t i = 0; i < maxFramesInFlight; ++i)
	{
		std::tie(uniformBuffers[i], uniformBuffersMemory[i]) = CreateBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	return std::make_tuple(uniformBuffers, uniformBuffersMemory);
}

// Create a descriptor pool from which we can allocate descriptor sets
VkDescriptorPool VulkanCore::CreateDescriptorPool(VkDevice logicalDevice, uint32_t maxFramesInFlight)
{
	std::vector<VkDescriptorPoolSize> poolSizes = // Which descriptor types descriptor sets are going to contain and how many of them
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxFramesInFlight },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxFramesInFlight }
	};

	VkDescriptorPoolCreateInfo poolInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(maxFramesInFlight),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}

	return descriptorPool;
}

// Create a descriptor set for each VkBuffer resource to bind it to the uniform buffer descriptor
std::vector<VkDescriptorSet> VulkanCore::CreateDescriptorSets(VkDevice logicalDevice, const std::vector<VkBuffer> &uniformBuffers, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, VkImageView textureImageView, VkSampler textureSampler, uint32_t maxFramesInFlight)
{
	std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout); // Same descriptor set layouts for all descriptor sets

	VkDescriptorSetAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool, // Descriptor pool to allocate from
		.descriptorSetCount = maxFramesInFlight, // The number of descriptor sets to allocate
		.pSetLayouts = layouts.data()
	};

	std::vector<VkDescriptorSet> descriptorSets(maxFramesInFlight);
	if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// The configuration of descriptors is updated using the vkUpdateDescriptorSets function, 
	// which takes an array of VkWriteDescriptorSet structs as parameter.
	for (size_t i = 0; i < maxFramesInFlight; ++i)
	{
		// Specify the buffer and the region within it that contains the data for the descriptor.
		VkDescriptorBufferInfo bufferInfo =
		{
			.buffer = uniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBufferObject)
		};

		VkDescriptorImageInfo imageInfo =
		{
			.sampler = textureSampler,
			.imageView = textureImageView, // Now a shader can access the image view
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		std::vector<VkWriteDescriptorSet> descriptorWrites =
		{
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i], // Descriptor set to update
				.dstBinding = 0, // Binding index of the uniform buffer; 0
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

				.pBufferInfo = &bufferInfo, // Array with descriptorCount structs that actually configure the descriptors
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i], // Descriptor set to update
				.dstBinding = 1, // Binding index of the texture sampler; 1
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

				.pImageInfo = &imageInfo
			}
		};

		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return descriptorSets;
}

void VulkanCore::UpdateUniformBuffer()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo =
	{
		.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // Identity matrix * rotation, axis
		.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // Eye position, target center position, up axis
		.proj = glm::perspective(glm::radians(45.0f), _swapChainExtent.width / (float)_swapChainExtent.height, 0.1f, 10.0f) // Vertical field of view, aspect ratio, clipping planes
	};
	// glm was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted.
	// Compensate this inversion.
	ubo.proj[1][1] *= -1;

	void *data;
	vkMapMemory(_logicalDevice, _uniformBuffersMemory[_currentFrame], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(_logicalDevice, _uniformBuffersMemory[_currentFrame]);
}

std::tuple<VkImage, VkDeviceMemory, VkImageView, uint32_t> VulkanCore::CreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string &texturePath)
{
	// Load a texture image
	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;
	stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) throw std::runtime_error(std::format("Failed to load {0}", texturePath));

	VkDeviceSize imageSize = texWidth * texHeight * 4; // Four bytes per pixel

	uint32_t textureMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1; // How many times the image can be divided by 2?

	// The most optimal memory for the GPU has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU.
	// We hire two buffers; one staging buffer in CPU-accessible memory to upload the data and the final buffer in device-local memory.
	// Then, use a buffer copy command to move the data from the staging buffer to the actual vertex buffer.
	// Create a buffer in host-visible memory so that we can use vkMapMemory and copy the pixels to it.
	auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(physicalDevice, logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data = nullptr;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data); // Map data <-> stagingBufferMemory
	memcpy(data, pixels, static_cast<size_t>(imageSize)); // Copy pixels -> data
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	// Clean up the original pixel array
	stbi_image_free(pixels);

	// Create an image object
	// Specify VK_IMAGE_USAGE_TRANSFER_SRC_BIT so that it can be used as a blit source
	auto [textureImage, textureImageMemory] = CreateImageAndMemory
	(
		physicalDevice,
		logicalDevice,
		texWidth, 
		texHeight, textureMipLevels, 
		VK_SAMPLE_COUNT_1_BIT, 
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT - The image is going to be used as a destination for the buffer copy from the staging buffer
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT - We will create mipmaps by transfering the image
		// VK_IMAGE_USAGE_SAMPLED_BIT - The texture will be sampled later
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	VkImageView textureImageView = CreateImageView(logicalDevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, textureMipLevels);

	// Before we use vkCmdCopyBufferToImage, the image must be in a proper layout.
	// Hence, transfer the image layout from undefined to transfer optimal
	
	// Image memory barriers are generally used to synchronize access to resources, but it can also be used to transition
	// image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.
	VkImageMemoryBarrier barrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

		// We want to transfer the image layout from oldLayout to newLayout.
		.srcAccessMask = 0, // Writes don't have to wait on anything
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

		// We don't want to transfer queue family ownership
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

		.image = textureImage,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = textureMipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Earliest possible pipeline stage
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // Pseudo-stage where transfers happen

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);
	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);

	// Now copy the buffer to the image
	CopyBufferToImage(logicalDevice, commandPool, graphicsQueue, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	// Generate mipmaps
	// Will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	GenerateMipmaps(physicalDevice, logicalDevice, commandPool, graphicsQueue, textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, textureMipLevels);

	return std::make_tuple(textureImage, textureImageMemory, textureImageView, textureMipLevels);
}

std::tuple<VkImage, VkDeviceMemory> VulkanCore::CreateImageAndMemory(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties)
{
	// Create an image object
	VkImageCreateInfo imageInfo =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D, // Texel coordinate system
		.format = imageFormat,
		.extent =
		{
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
			.depth = 1
		},
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = numSamples,
		.tiling = imageTiling,
		.usage = imageUsage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE, // Will be used by one queue family that supports graphics transfer operations
		// Not usable by the GPU and the first transition will discard the texels
		// We will transition the image to a transfer destination and then copy texel data to it
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VkImage image = VK_NULL_HANDLE;
	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image.");
	}

	// Allocate memory for the image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, memoryProperties)
	};

	VkDeviceMemory imageMemory = VK_NULL_HANDLE;
	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate image memory.");
	}

	// Bind the image and the memory
	vkBindImageMemory(logicalDevice, image, imageMemory, 0);

	return std::make_tuple(image, imageMemory);
}

// Start a temporary command buffer
VkCommandBuffer VulkanCore::BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT // This buffer is going to be used just once.
	};

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanCore::EndSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue submitQueue)
{
	vkEndCommandBuffer(commandBuffer);

	// Execute the command buffer to complete the transfer
	VkSubmitInfo submitInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer
	};

	vkQueueSubmit(submitQueue, 1, &submitInfo, VK_NULL_HANDLE); // Submit to the queue
	vkQueueWaitIdle(submitQueue); // Wait for operations in the command queue to be finished.

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void VulkanCore::CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region =
	{
		.bufferOffset = 0,
		.bufferRowLength = 0, // Pixels are tightly packed
		.bufferImageHeight = 0,

		// Which part of the image is copied
		.imageSubresource =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { width, height, 1 }
	};

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);
	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
}

VkImageView VulkanCore::CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewInfo =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,

		// What the image's purpose is
		.subresourceRange =
		{
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	VkImageView imageView;
	if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture image view.");
	}

	return imageView;
}

// Textures are accessed through samplers so that filters and transformations are applied to get rid of artifacts.
VkSampler VulkanCore::CreateTextureSampler(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t textureMipLevels)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	// Specify filters and transformations to apply
	VkSamplerCreateInfo samplerInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR, // Deals with oversampling
		.minFilter = VK_FILTER_LINEAR, // Deals with undersampling

		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,

		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

		.mipLodBias = 0.0f,

		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,

		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,

		.minLod = 0.0f,
		.maxLod = static_cast<float>(textureMipLevels),

		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE, // Use [0, 1) range on all axes
	};

	VkSampler textureSampler = VK_NULL_HANDLE;
	if (vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture sampler.");
	}

	return textureSampler;
}

std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanCore::CreateDepthResources(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkExtent2D swapChainExtent)
{
	VkFormat depthFormat = FindSupportedFormat(physicalDevice, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); // Find suitable format for a depth image
	auto [depthImage, depthImageMemory] = CreateImageAndMemory
	(
		physicalDevice,
		logicalDevice,
		swapChainExtent.width,
		swapChainExtent.height,
		1,
		GetMaxUsableSampleCount(physicalDevice),
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	VkImageView depthImageView = CreateImageView(logicalDevice, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	return std::make_tuple(depthImage, depthImageMemory, depthImageView);
}

VkFormat VulkanCore::FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags desiredFeatures)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & desiredFeatures) == desiredFeatures)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & desiredFeatures) == desiredFeatures)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a supported format.");
}

std::tuple<std::vector<Vertex>, std::vector<uint32_t>> VulkanCore::LoadModel(const std::string &modelPath)
{
	tinyobj::attrib_t attribute; // Holds all of the positions, normals, and texture coordinates
	std::vector<tinyobj::shape_t> shapes; // Contains all of the separate objects and their faces
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &warn, &err, modelPath.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex =
			{
				.pos =
				{
					attribute.vertices[3 * index.vertex_index + 0],
					attribute.vertices[3 * index.vertex_index + 1],
					attribute.vertices[3 * index.vertex_index + 2]
				},
				.color = { 1.0f, 1.0f, 1.0f },
				.texCoord =
				{
					attribute.texcoords[2 * index.texcoord_index + 0],
					1.0f - attribute.texcoords[2 * index.texcoord_index + 1] // Difference between the texture coordinate system
				}
			};

			if (!uniqueVertices.contains(vertex))
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}
			indices.push_back(uniqueVertices[vertex]);
		}
	}

	return std::make_tuple(std::move(vertices), std::move(indices));
}

void VulkanCore::GenerateMipmaps(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if the image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("The texture image format does not support linear blitting.");
	}

	// repeatedly blit a source image to a smaller one
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);

	VkImageMemoryBarrier barrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;
	// (i - 1)'th layout
	// 1. VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	// 2. Blit
	// 3. VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	for (uint32_t i = 1; i < mipLevels; ++i)
	{
		barrier.subresourceRange.baseMipLevel = i - 1; // Process (i - 1)'th mip level
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Transition level (i - 1) to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier); // Wait for level i - 1 to be filled with previous blit command or vkCmdCopyBufferToImage

		// Blit area
		VkImageBlit blit =
		{
			.srcSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i - 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.srcOffsets = { { 0, 0, 0 }, { mipWidth, mipHeight, 1 } },
			.dstSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.dstOffsets = { { 0, 0, 0 }, { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 } }
		};

		vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR); // Record the blit command - both the source and the destination images are same, because they are blitting between different mip levels.

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// This transition to the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL flag waits on the current blit command to finish.
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		// Divide the dimensions with a guarantee that they won't be 0
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition the last mipmap
	barrier.subresourceRange.baseMipLevel = mipLevels - 1; // Process the last mipmap
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
}

VkSampleCountFlagBits VulkanCore::GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice) // Return min(depth sample count, color sample count)
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags sampleCountFlag = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	std::vector<VkSampleCountFlagBits> countFlags = { VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_1_BIT };
	for (const auto flag : countFlags)
	{
		if (sampleCountFlag & flag) return flag;
	}

	return VK_SAMPLE_COUNT_1_BIT;
}

// Create an offsreen buffer for MSAA
// Since there is only one ongoing rendering operation, create only one such buffer.
std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanCore::CreateColorResources(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkFormat swapChainImageFormat, VkExtent2D swapChainExtent)
{
	VkFormat colorFormat = swapChainImageFormat;

	auto [colorImage, colorImageMemory] = CreateImageAndMemory
	(
		physicalDevice,
		logicalDevice,
		swapChainExtent.width,
		swapChainExtent.height,
		1,
		GetMaxUsableSampleCount(physicalDevice),
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL, // Texels are laid out in an implementation-defined order for optimal access.
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // Will be used as a color attachment
		// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - Will stay on the GPU and not accessible by the CPU
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	VkImageView colorImageView = CreateImageView(logicalDevice, colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	return std::make_tuple(colorImage, colorImageMemory, colorImageView);
}

VkDescriptorPool VulkanCore::CreateImGuiDescriptorPool(VkDevice logicalDevice, uint32_t maxFramesInFlight)
{
	std::vector<VkDescriptorPoolSize> poolSizes = // Which descriptor types the descriptor sets are going to contain and how many of them?
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = maxFramesInFlight,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a ImGui descriptor pool.");
	}

	return descriptorPool;
}

VkRenderPass VulkanCore::CreateImGuiRenderPass(VkDevice logicalDevice, VkFormat swapChainImageFormat)
{
	VkAttachmentDescription colorAttachment =
	{
		.format = swapChainImageFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT, // ImGui looks fine without MSAA
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // Do not clear the the content of the framebuffer - we want the GUI to be drawn over the main rendering!
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE, // It will be read later to be drawn.
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // We don't need any stencil operations for ImGui
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Same
		.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // For optimal performance 
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR // Source for the presentation using swap chains
	};

	VkAttachmentReference colorAttachmentRef =
	{
		.attachment = 0, // Index of the color attachment (referenced from the fragment shader with the 'layout(location = 0) out vec4 outColor' directive)
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // Optimal for the attachment to function as a color buffer
	};

	VkSubpassDescription subpass =
	{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, // Graphics subpass, not a compute subpass
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef
	};

	VkSubpassDependency dependency =
	{
		.srcSubpass = VK_SUBPASS_EXTERNAL, // Index of the implicit subpass before the render pass
		.dstSubpass = 0, // Index of our subpass

		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, // Stages that are being waited for
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, // Stages that are waiting

		.srcAccessMask = 0, // Operations to wait on
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT // Operations that are waiting
	};

	VkRenderPassCreateInfo renderPassInfo =
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorAttachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	VkRenderPass renderPass = VK_NULL_HANDLE;
	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an ImGui render pass.");
	}

	return renderPass;
}

std::vector<VkFramebuffer> VulkanCore::CreateImGuiFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D swapChainExtent, const std::vector<VkImageView> &swapChainImageViews)
{
	std::vector<VkFramebuffer> framebuffers(swapChainImageViews.size());

	for (size_t i = 0; i < framebuffers.size(); ++i)
	{
		VkFramebufferCreateInfo framebufferInfo =
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = 1,
			.pAttachments = &swapChainImageViews[i],
			.width = swapChainExtent.width,
			.height = swapChainExtent.height,
			.layers = 1
		};

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create an ImGui framebuffer.");
		}
	}

	return framebuffers;
}

void VulkanCore::UpdateImGuiCommandBuffer(uint32_t imageIndex)
{
	// Create a frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame(); // Handle user imputs, the screen size etc.
	ImGui::NewFrame(); // Initialize an actual ImGuiFrame

	ImGui::ShowDemoWindow();

	ImGui::Render();
	ImDrawData *mainDrawData = ImGui::GetDrawData();

	// Begin the command buffer
	VkCommandBufferBeginInfo commandBufferBeginInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	if (vkBeginCommandBuffer(_ImGuiCommandBuffers[imageIndex], &commandBufferBeginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a command buffer.");
	}

	// Begin the render pass
	VkRenderPassBeginInfo renderPassBeginInfo =
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = _ImGuiRenderPass,
		.framebuffer = _ImGuiFramebuffers[imageIndex],
		.renderArea =
		{
			.extent = _swapChainExtent
		}
	};

	vkCmdBeginRenderPass(_ImGuiCommandBuffers[imageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	ImGui_ImplVulkan_RenderDrawData(mainDrawData, _ImGuiCommandBuffers[imageIndex]); // Add draw calls to the render pass
	vkCmdEndRenderPass(_ImGuiCommandBuffers[imageIndex]);

	// End the command buffer
	if (vkEndCommandBuffer(_ImGuiCommandBuffers[imageIndex]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to begin a command buffer.");
	}
}

void VulkanCore::CleanUpImGui()
{
	for (auto framebuffer : _ImGuiFramebuffers)
	{
		vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
	}

	vkDestroyRenderPass(_logicalDevice, _ImGuiRenderPass, nullptr);

	vkFreeCommandBuffers(_logicalDevice, _ImGuiCommandPool, static_cast<uint32_t>(_ImGuiCommandBuffers.size()), _ImGuiCommandBuffers.data());
	vkDestroyCommandPool(_logicalDevice, _ImGuiCommandPool, nullptr);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyDescriptorPool(_logicalDevice, _ImGuiDescriptorPool, nullptr);
}

void VulkanCore::RecreateImGuiSwapChainComponents()
{
	for (auto &framebuffer : _ImGuiFramebuffers)
	{
		vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
	}

	_ImGuiFramebuffers = CreateImGuiFramebuffers(_logicalDevice, _ImGuiRenderPass, _swapChainExtent, _swapChainImageViews);
}