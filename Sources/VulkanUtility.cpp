#include "VulkanUtility.h"

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &requiredProperties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties); // Query info about the available types of memory

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties) // Check if the memory satisfies the condition
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find a suitable memory type.");
}

std::tuple<VkBuffer, VkDeviceMemory> CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties)
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

void CopyBuffer(VkDevice logicalDevice, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool, VkQueue graphicsQueue)
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

VkCommandBuffer BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool)
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

void EndSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue submitQueue)
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

void CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
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

std::tuple<VkImage, VkDeviceMemory> CreateImageAndMemory(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties)
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

VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char> &code)
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

VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice) // Return min(depth sample count, color sample count)
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

VkImageView CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
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

	VkImageView imageView = VK_NULL_HANDLE;
	if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture image view.");
	}

	return imageView;
}

std::vector<char> ReadFile(const std::string &fileName)
{
	std::ifstream file(fileName, std::ios::ate | std::ios::binary);

	if (!file.is_open()) throw std::runtime_error(std::format("Failed to open the file {0}", fileName));

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}


