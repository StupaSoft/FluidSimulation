#include "VulkanResources.h"

// Helper functions
std::vector<Buffer> CreateBuffers(size_t bufferSize, size_t maxFramesInFlight, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryProperty)
{
	Memory memory = CreateMemory(memoryProperty);

	std::vector<Buffer> buffers(maxFramesInFlight);
	for (size_t i = 0; i < maxFramesInFlight; ++i)
	{
		buffers[i] = CreateBuffer(bufferSize, bufferUsage);
	}
	memory->Bind(buffers);

	return buffers;
}

// Memory
DeviceMemory::~DeviceMemory()
{
	vkFreeMemory(VulkanCore::Get()->GetLogicalDevice(), _memory, nullptr);
}

void DeviceMemory::Bind(const std::vector<Buffer> &buffers)
{
	// Allocate a single, large memory block for the buffers
	std::vector<VkDeviceSize> bufferSizes;
	uint32_t memoryTypeBits = 0;

	for (const auto &buffer : buffers)
	{
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(VulkanCore::Get()->GetLogicalDevice(), buffer->GetBufferHandle(), &memRequirements); // Query memory requirements

		bufferSizes.push_back(memRequirements.size);
		memoryTypeBits |= memRequirements.memoryTypeBits;
	}

	_size = std::accumulate(bufferSizes.begin(), bufferSizes.end(), 0u);
	VkMemoryAllocateInfo allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = _size,
		.memoryTypeIndex = FindMemoryType(VulkanCore::Get()->GetPhysicalDevice(), memoryTypeBits, _properties)
	};

	if (vkAllocateMemory(VulkanCore::Get()->GetLogicalDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate buffer memory.");
	}

	// Associate this memory block with the buffers
	VkDeviceSize offset = 0;
	for (size_t i = 0; i < buffers.size(); ++i)
	{
		Buffer buffer = buffers[i];

		vkBindBufferMemory(VulkanCore::Get()->GetLogicalDevice(), buffer->GetBufferHandle(), _memory, offset);
		buffer->SetMemory(shared_from_this(), offset);

		offset += bufferSizes[i];
	}
}

void DeviceMemory::Bind(ImageResource *image)
{
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanCore::Get()->GetLogicalDevice(), image->GetImageHandle(), &memRequirements);

	VkMemoryAllocateInfo allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(VulkanCore::Get()->GetPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	if (vkAllocateMemory(VulkanCore::Get()->GetLogicalDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate image memory.");
	}

	vkBindImageMemory(VulkanCore::Get()->GetLogicalDevice(), image->GetImageHandle(), _memory, 0); // Bind the image and the memory
	image->SetMemory(shared_from_this());
}

// Buffer
BufferResource::BufferResource(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage) :
	_size(bufferSize)
{
	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize, // The size of the buffer in bytes
		.usage = bufferUsage, // The purpose of the buffer
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	if (vkCreateBuffer(VulkanCore::Get()->GetLogicalDevice(), &bufferInfo, nullptr, &_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a buffer.");
	}
}

BufferResource::~BufferResource()
{
	if (_mappedMemory != nullptr)
	{
		if (_memory->IsDeviceLocal())
		{
			vkUnmapMemory(VulkanCore::Get()->GetLogicalDevice(), _stagingBuffer->GetMemory()->GetMemoryHandle());
		}
		else
		{
			vkUnmapMemory(VulkanCore::Get()->GetLogicalDevice(), _memory->GetMemoryHandle());
		}
	}

	vkDestroyBuffer(VulkanCore::Get()->GetLogicalDevice(), _buffer, nullptr);
}

void BufferResource::CopyFrom(const void *source, VkDeviceSize copyOffset, VkDeviceSize copySize)
{
	if (copySize == VK_WHOLE_SIZE) copySize = _size;

	if (_memory->IsDeviceLocal())
	{
		if (_mappedMemory == nullptr)
		{
			// If we haven't mapped memory with the buffer, first create a staging buffer to map.
			// The most optimal memory for the GPU to access has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU.
			// We hire two buffers; one staging buffer in CPU-accessible memory to upload the data and the final buffer in device-local memory.
			// Then, use a buffer copy command to move the data from the staging buffer to the actual vertex buffer.
			// Create a buffer in host-visible memory so that we can use vkMapMemory and copy the pixels to it.

			_stagingBuffer = CreateBuffer(_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
			Memory stagingMemory = CreateMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			stagingMemory->Bind({ _stagingBuffer });

			vkMapMemory(VulkanCore::Get()->GetLogicalDevice(), _stagingBuffer->GetMemory()->GetMemoryHandle(), 0, _size, 0, &_mappedMemory); // Map data <-> stagingBufferMemory
		}

		memcpy(_mappedMemory, source, _stagingBuffer->Size());
		CopyFrom(_stagingBuffer, copyOffset, copySize);
	}
	else
	{
		if (_mappedMemory == nullptr)
		{
			vkMapMemory(VulkanCore::Get()->GetLogicalDevice(), _memory->GetMemory(), 0, _size, 0, &_mappedMemory);
		}
		
		void *offsetData = reinterpret_cast<std::byte *>(_mappedMemory) + copyOffset;
		const void *offsetSource = reinterpret_cast<const std::byte *>(source) + copyOffset;

		memcpy(offsetData, offsetSource, copySize);
	}
}

void BufferResource::CopyFrom(const Buffer &source, VkDeviceSize copyOffset, VkDeviceSize copySize)
{
	if (copySize == VK_WHOLE_SIZE) copySize = _size;

	if (_memory == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Memory has not been bound for this buffer.");
	}

	// Memory transfer operations are executed using command buffers.
	VkCommandBuffer commandBuffer = VulkanCore::Get()->BeginSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool());

	// Now add a copy command
	VkBufferCopy copyRegion
	{
		.srcOffset = copyOffset,
		.dstOffset = copyOffset,
		.size = copySize,
	};

	vkCmdCopyBuffer(commandBuffer, source->GetBufferHandle(), _buffer, 1, &copyRegion);

	VulkanCore::Get()->EndSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool(), commandBuffer, VulkanCore::Get()->GetGraphicsQueue());
}

void BufferResource::SetMemory(const Memory &memory, VkDeviceSize offset)
{
	_memory = memory;
	_offsetWithinMemory = offset;
}

// Image
ImageResource::ImageResource(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageAspectFlags aspectFlags) :
	_width(width),
	_height(height)
{
	// Create an image object
	VkImageCreateInfo imageInfo
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

	if (vkCreateImage(VulkanCore::Get()->GetLogicalDevice(), &imageInfo, nullptr, &_image) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image.");
	}

	// Bind memory
	// We should bind memory before binding an image view
	_memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_memory->Bind(this);

	// Create an image view
	VkImageViewCreateInfo viewInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = _image,
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

	if (vkCreateImageView(VulkanCore::Get()->GetLogicalDevice(), &viewInfo, nullptr, &_imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture image view.");
	}
}

ImageResource::~ImageResource()
{
	if (_mappedMemory != nullptr)
	{
		if (_memory->IsDeviceLocal())
		{
			vkUnmapMemory(VulkanCore::Get()->GetLogicalDevice(), _stagingBuffer->GetMemory()->GetMemoryHandle());
		}
		else
		{
			vkUnmapMemory(VulkanCore::Get()->GetLogicalDevice(), _memory->GetMemoryHandle());
		}
	}

	if (!_isSwapChainImage) vkDestroyImage(VulkanCore::Get()->GetLogicalDevice(), _image, nullptr); // Swap chain images will be destroyed through vkDestroySwapchainKHR
	vkDestroyImageView(VulkanCore::Get()->GetLogicalDevice(), _imageView, nullptr);
}

void ImageResource::CopyFrom(const void *source, uint32_t width, uint32_t height)
{
	// Images are always device local
	if (_mappedMemory == nullptr)
	{
		_stagingBuffer = CreateBuffer(Size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		Memory stagingMemory = CreateMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		stagingMemory->Bind({ _stagingBuffer });

		vkMapMemory(VulkanCore::Get()->GetLogicalDevice(), _stagingBuffer->GetMemory()->GetMemoryHandle(), 0, Size(), 0, &_mappedMemory); // Map data <-> stagingBufferMemory
	}

	memcpy(_mappedMemory, source, _stagingBuffer->Size());
	CopyFrom(_stagingBuffer, width, height);
}

void ImageResource::CopyFrom(const Buffer &buffer, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region
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

	VkCommandBuffer commandBuffer = VulkanCore::Get()->BeginSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool());
	vkCmdCopyBufferToImage(commandBuffer, buffer->GetBufferHandle(), _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	VulkanCore::Get()->EndSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool(), commandBuffer, VulkanCore::Get()->GetGraphicsQueue());
}

void ImageResource::SetMemory(const Memory &memory)
{
	_memory = memory;
}
