#include "VulkanCore.h"

const uint32_t VulkanCore::MAX_FRAMES_IN_FLIGHT = 2;

// ======================================== Interface ========================================
VulkanCore::~VulkanCore()
{
	CleanUp();
}

void VulkanCore::InitVulkan()
{
	// Global resources
	_instance = CreateInstance(ENABLE_VALIDATION_LAYERS, VALIDATION_LAYERS);

	_debugMessenger = CreateDebugMessenger(_instance, ENABLE_VALIDATION_LAYERS);

	_surface = CreateSurface(_instance, _window); // Must be created right after the instance
	_physicalDevice = SelectPhysicalDevice(_instance, _surface, DEVICE_EXTENSIONS);
	std::tie(_logicalDevice, _graphicsQueue, _presentQueue) = CreateLogicalDevice(_physicalDevice, _surface, ENABLE_VALIDATION_LAYERS, VALIDATION_LAYERS, DEVICE_EXTENSIONS);

	std::tie(_swapChain, _swapChainImages, _swapChainImageFormat, _swapChainExtent) = CreateSwapChain(_physicalDevice, _logicalDevice, _surface, _window);
	_swapChainImageViews = CreateImageViews(_logicalDevice, _swapChainImages, _swapChainImageFormat);

	_renderPass = CreateRenderPass(_swapChainImageFormat);

	std::tie(_colorImage, _colorImageMemory, _colorImageView) = CreateColorResources(_swapChainImageFormat, _swapChainExtent);
	std::tie(_depthImage, _depthImageMemory, _depthImageView) = CreateDepthResources(_swapChainExtent);
	_frameBuffers = CreateFramebuffers(_logicalDevice, _renderPass, _swapChainExtent, _swapChainImageViews, { _depthImageView, _colorImageView });

	_commandPool = CreateCommandPool(_physicalDevice, _logicalDevice, _surface);
	_commandBuffers = CreateCommandBuffers(_logicalDevice, _commandPool, MAX_FRAMES_IN_FLIGHT);
	std::tie(_imageAvailableSemaphores, _renderFinishedSemaphores, _inFlightFences) = CreateSyncObjects(MAX_FRAMES_IN_FLIGHT);
}

void VulkanCore::RemoveModel(const std::shared_ptr<ModelBase> &model)
{
	auto it = std::find(_models.begin(), _models.end(), model);
}

void VulkanCore::DrawFrame()
{
	vkWaitForFences(_logicalDevice, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX); // Wait until the previous frame has finished

	uint32_t imageIndex = 0; // Index to the image in the swap chain
	VkResult result = vkAcquireNextImageKHR(_logicalDevice, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex); // Get an available swap chain image that has become available
	if (result == VK_ERROR_OUT_OF_DATE_KHR) // The swap chain has become incompatible with the surface; usually after resizing the window
	{
		RecreateSwapChain();
		_currentFrame = 0;

		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) // Even if the result is suboptimal, proceed anyway since we have already got an image.
	{
		throw std::runtime_error("Failed to acquire a swap chain image.");
	}

	vkResetFences(_logicalDevice, 1, &_inFlightFences[_currentFrame]); // Reset the fece only if we are submitting a work

	vkResetCommandBuffer(_commandBuffers[_currentFrame], 0);
	RecordCommandBuffer(_swapChainExtent, _renderPass, _frameBuffers[_currentFrame], _commandBuffers[_currentFrame], _currentFrame);

	// Submit the command buffer
	std::vector<VkCommandBuffer> submitCommandBuffers = { _commandBuffers[_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Wait with writing colors to the image until it's available
	VkSubmitInfo submitInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_imageAvailableSemaphores[_currentFrame],
		.pWaitDstStageMask = waitStages,

		.commandBufferCount = 1,
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

void VulkanCore::SetUpScene()
{
	_mainCamera = std::make_unique<Camera>(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), FOV_Y, _swapChainExtent.width, _swapChainExtent.height);
	_mainLight = std::make_unique<DirectionalLight>(glm::vec3(-1.0f, -5.0f, -5.0f), glm::vec3(1.0f, 1.0f, 1.0f), 2.0f);	
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
	_onCleanUpSwapChain.Invoke();

	vkDestroyRenderPass(_logicalDevice, _renderPass, nullptr);

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
// Set physicalDevice to a suitable physical device
VkPhysicalDevice VulkanCore::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char *> &deviceExtensions)
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

// Find queues that support graphics commands
QueueFamilyIndices VulkanCore::FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const
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

// Return supported formats and present modes 
SwapChainSupportDetails VulkanCore::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const
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
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(_logicalDevice);

	CleanUpSwapChain();

	std::tie(_swapChain, _swapChainImages, _swapChainImageFormat, _swapChainExtent) = CreateSwapChain(_physicalDevice, _logicalDevice, _surface, _window);
	_swapChainImageViews = CreateImageViews(_logicalDevice, _swapChainImages, _swapChainImageFormat);

	_mainCamera->SetFOV(FOV_Y);
	_mainCamera->SetExtent(_swapChainExtent.width, _swapChainExtent.height);

	std::tie(_colorImage, _colorImageMemory, _colorImageView) = CreateColorResources(_swapChainImageFormat, _swapChainExtent);
	std::tie(_depthImage, _depthImageMemory, _depthImageView) = CreateDepthResources(_swapChainExtent);
	_frameBuffers = CreateFramebuffers(_logicalDevice, _renderPass, _swapChainExtent, _swapChainImageViews, { _depthImageView, _colorImageView });

	_onRecreateSwapChain.Invoke();
}

void VulkanCore::CleanUpSwapChain()
{
	_onCleanUpSwapChain.Invoke();

	vkDestroyImageView(_logicalDevice, _depthImageView, nullptr);
	vkDestroyImage(_logicalDevice, _depthImage, nullptr);
	vkFreeMemory(_logicalDevice, _depthImageMemory, nullptr);

	vkDestroyImageView(_logicalDevice, _colorImageView, nullptr);
	vkDestroyImage(_logicalDevice, _colorImage, nullptr);
	vkFreeMemory(_logicalDevice, _colorImageMemory, nullptr);

	for (auto &framebuffer : _frameBuffers)
	{
		vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
	}

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
		swapChainImageViews[i] = CreateImageView(_logicalDevice, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	return swapChainImageViews;
}

VkRenderPass VulkanCore::CreateRenderPass(VkFormat swapChainImageFormat)
{
	// Before creating the pipeline, we need to tell Vulkan about the framebuffer attachments that will be used while rendering
	// Specify how many color and depth buffers there will be, how many samples to use for each them...

	// Create a depth buffer attachment
	VkAttachmentDescription depthAttachment =
	{
		.format = FindSupportedFormat(_physicalDevice, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT), // Should be the same as the depth image itself
		.samples = GetMaxUsableSampleCount(_physicalDevice),
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
		.samples = GetMaxUsableSampleCount(_physicalDevice),
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
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
	if (vkCreateRenderPass(_logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
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

void VulkanCore::RecordCommandBuffer(VkExtent2D swapChainExtent, VkRenderPass renderPass, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, uint32_t currentFrame)
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

	// Now begin the render pass
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		// Set the viewport and the scissor
		VkViewport viewport =
		{
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)swapChainExtent.width,
			.height = (float)swapChainExtent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f

		};
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor =
		{
			.offset = { 0, 0 },
			.extent = swapChainExtent
		};
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// Draw models here
		ForAllModels(&ModelBase::RecordCommand, commandBuffer, currentFrame);
	}
	vkCmdEndRenderPass(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to record command a buffer.");
	}
}

std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> VulkanCore::CreateSyncObjects(uint32_t maxFramesInFlight)
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
			vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS;
		if (failed)
		{
			throw std::runtime_error("Failed to create semaphores.");
		}
	}

	return std::make_tuple(imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences);
}

std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanCore::CreateDepthResources(VkExtent2D swapChainExtent)
{
	VkFormat depthFormat = FindSupportedFormat(_physicalDevice, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); // Find suitable format for a depth image
	auto [depthImage, depthImageMemory] = CreateImageAndMemory
	(	
		_physicalDevice,
		_logicalDevice,
		swapChainExtent.width,
		swapChainExtent.height,
		1,
		GetMaxUsableSampleCount(_physicalDevice),
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkImageView depthImageView = CreateImageView(_logicalDevice, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

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

// Create an offsreen buffer for MSAA
// Since there is only one ongoing rendering operation, create only one such buffer.
std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanCore::CreateColorResources(VkFormat swapChainImageFormat, VkExtent2D swapChainExtent)
{
	VkFormat colorFormat = swapChainImageFormat;

	auto [colorImage, colorImageMemory] = CreateImageAndMemory
	(	
		_physicalDevice,
		_logicalDevice,
		swapChainExtent.width,
		swapChainExtent.height,
		1,
		GetMaxUsableSampleCount(_physicalDevice),
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		// Texels are laid out in an implementation-defined order for optimal access.
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // Will be used as a color attachment
		// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - Will stay on the GPU and not accessible by the CPU
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkImageView colorImageView = CreateImageView(_logicalDevice, colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	return std::make_tuple(colorImage, colorImageMemory, colorImageView);
}