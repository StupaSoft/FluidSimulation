#include "VulkanUtility.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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

Buffer CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties)
{
	VkBufferCreateInfo bufferInfo =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize, // The size of the buffer in bytes
		.usage = bufferUsage, // The purpose of the buffer
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkBuffer bufferHandle = VK_NULL_HANDLE;
	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &bufferHandle) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a buffer.");
	}
	// Memory bound to the buffer has not been assigned to the buffer until here

	// Assign memory
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, bufferHandle, &memRequirements); // Query memory requirements

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
	vkBindBufferMemory(logicalDevice, bufferHandle, bufferMemory, 0);

	Buffer buffer(new BufferMemory{ logicalDevice, bufferHandle, bufferMemory, bufferSize }, BufferDeleter());

	return buffer;
}

std::vector<Buffer> CreateBuffers(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, size_t bufferSize, size_t maxFramesInFlight, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperty)
{
	std::vector<Buffer> buffers(maxFramesInFlight);
	for (size_t i = 0; i < maxFramesInFlight; ++i)
	{
		buffers[i] = CreateBuffer(physicalDevice, logicalDevice, bufferSize, usage, memoryProperty);
	}

	return buffers;
}

void CopyBufferToBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, Buffer srcBuffer, Buffer dstBuffer, VkDeviceSize size)
{
	// Memory transfer operations are executed using command buffers.
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);

	// Now add a copy command
	VkBufferCopy copyRegion
	{
		.size = size
	};
	vkCmdCopyBuffer(commandBuffer, srcBuffer->_buffer, dstBuffer->_buffer, 1, &copyRegion);

	EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
}

void CopyMemoryToBuffer(VkDevice logicalDevice, const void *source, Buffer buffer, VkDeviceSize copyOffset, VkDeviceSize copySize)
{
	if (copySize == -1) copySize = buffer->_size;

	const std::byte *offsetPtr = reinterpret_cast<const std::byte *>(source) + copyOffset;
	source = offsetPtr;

	void *data;
	vkMapMemory(logicalDevice, buffer->_memory, copyOffset, copySize, 0, &data);
	memcpy(data, source, copySize);
	vkUnmapMemory(logicalDevice, buffer->_memory);
}

void CopyMemoryToBuffers(VkDevice logicalDevice, const void *source, const std::vector<Buffer> &buffers, VkDeviceSize copyOffset, VkDeviceSize copySize)
{
	if (copySize == -1) copySize = buffers[0]->_size;

	const std::byte *offsetPtr = reinterpret_cast<const std::byte *>(source) + copyOffset;
	source = offsetPtr;

	for (size_t i = 0; i < buffers.size(); ++i)
	{
		void *data;
		vkMapMemory(logicalDevice, buffers[i]->_memory, copyOffset, copySize, 0, &data);
		memcpy(data, source, copySize);
		vkUnmapMemory(logicalDevice, buffers[i]->_memory);
	}
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

void CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, Buffer buffer, Image image, uint32_t width, uint32_t height)
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
	vkCmdCopyBufferToImage(commandBuffer, buffer->_buffer, image->_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
}

Image CreateImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties, VkImageAspectFlags aspectFlags)
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

	VkImage imageHandle = VK_NULL_HANDLE;
	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &imageHandle) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image.");
	}

	// Allocate memory for the image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(logicalDevice, imageHandle, &memRequirements);

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
	vkBindImageMemory(logicalDevice, imageHandle, imageMemory, 0);

	// Create an image view
	VkImageViewCreateInfo viewInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = imageHandle,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageFormat,

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

	Image image(new ImageMemoryView{ logicalDevice, imageHandle, imageMemory, imageView }, ImageDeleter());

	return image;
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

std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::string &OBJPath)
{
	tinyobj::attrib_t attribute; // Holds all of the positions, normals, and texture coordinates
	std::vector<tinyobj::shape_t> shapes; // Contains all of the separate objects and their faces
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &warn, &err, OBJPath.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

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
				.normal =
				{
					attribute.normals[3 * index.normal_index + 0],
					attribute.normals[3 * index.normal_index + 1],
					attribute.normals[3 * index.normal_index + 2]
				},
				.texCoord =
				{
					attribute.texcoords[2 * index.texcoord_index + 0],
					1.0f - attribute.texcoords[2 * index.texcoord_index + 1] // Reflect the difference between the texture coordinate system
				}
			};

			indices.push_back(static_cast<uint32_t>(vertices.size()));
			vertices.push_back(vertex);
		}
	}

	return std::make_tuple(std::move(vertices), std::move(indices));
}

std::tuple<Image, uint32_t> CreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string &texturePath)
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
	Buffer stagingBuffer = CreateBuffer(physicalDevice, logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data = nullptr;
	vkMapMemory(logicalDevice, stagingBuffer->_memory, 0, imageSize, 0, &data); // Map data <-> stagingBufferMemory
	memcpy(data, pixels, static_cast<size_t>(imageSize)); // Copy pixels -> data
	vkUnmapMemory(logicalDevice, stagingBuffer->_memory);

	// Clean up the original pixel array
	stbi_image_free(pixels);

	// Create an image object
	// Specify VK_IMAGE_USAGE_TRANSFER_SRC_BIT so that it can be used as a blit source
	Image texture = CreateImage
	(
		physicalDevice,
		logicalDevice,
		texWidth,
		texHeight,
		textureMipLevels, 
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT - The image is going to be used as a destination for the buffer copy from the staging buffer
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT - We will create mipmaps by transfering the image
		// VK_IMAGE_USAGE_SAMPLED_BIT - The texture will be sampled later
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

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

		.image = texture->_image,
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
	CopyBufferToImage(logicalDevice, commandPool, graphicsQueue, stagingBuffer, texture, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	// Generate mipmaps
	// Will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	GenerateMipmaps(physicalDevice, logicalDevice, commandPool, graphicsQueue, texture, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, textureMipLevels);

	return std::make_tuple(texture, textureMipLevels);
}

void GenerateMipmaps(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, Image image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
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
		.image = image->_image,
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

		vkCmdBlitImage(commandBuffer, image->_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image->_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR); // Record the blit command - both the source and the destination images are same, because they are blitting between different mip levels.

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

std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout)
{
	return CreateComputePipeline(logicalDevice, computeShaderModule, descriptorSetLayout, {});
}

std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges)
{
	VkPipelineShaderStageCreateInfo computeShaderStageInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = computeShaderModule,
		.pName = "main"
	};

	const VkPushConstantRange *pushConstantRangesPtr = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();
	VkPipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout,
		.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
		.pPushConstantRanges = pushConstantRangesPtr
	};

	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &computePipelineLayout))
	{
		throw std::runtime_error("Failed to create a compute pipeline layout.");
	}

	VkComputePipelineCreateInfo pipelineInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = computeShaderStageInfo,
		.layout = computePipelineLayout
	};

	VkPipeline computePipeline = VK_NULL_HANDLE;
	if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline))
	{
		throw std::runtime_error("Failed to create a compute pipeline.");
	}

	return std::make_tuple(computePipeline, computePipelineLayout);
}

size_t DivisionCeil(size_t x, size_t y) 
{ 
	return (x + y - 1) / y; 
};