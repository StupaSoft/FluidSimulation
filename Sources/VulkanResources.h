#pragma once

#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanCore.h"

// Declarations and aliases
class DeviceMemory;
using Memory = std::shared_ptr<DeviceMemory>;

class BufferResource;
using Buffer = std::shared_ptr<BufferResource>;

class ImageResource;
using Image = std::shared_ptr<ImageResource>;

// Instantiation helper functions
inline Memory CreateMemory(VkMemoryPropertyFlags properties) { return std::make_shared<DeviceMemory>(properties); }
inline Buffer CreateBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage) { return std::make_shared<BufferResource>(bufferSize, bufferUsage); }
inline Image CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageAspectFlags aspectFlags) 
{ 
	return std::make_shared<ImageResource>(width, height, mipLevels, numSamples, imageFormat, imageTiling, imageUsage, aspectFlags);
}
inline Image CreateSwapchainImage(VkImage image, VkImageView imageView) { return std::make_shared<ImageResource>(image, imageView); }

std::vector<Buffer> CreateBuffers(size_t bufferSize, size_t maxFramesInFlight, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryProperty);

// Memory
class DeviceMemory : public std::enable_shared_from_this<DeviceMemory>
{
private:
	VkMemoryPropertyFlags _properties = 0;
	VkDeviceMemory _memory = VK_NULL_HANDLE;

	VkDeviceSize _size = 0;

public:
	DeviceMemory(VkMemoryPropertyFlags properties) : _properties(properties) {}
	~DeviceMemory();

	auto Size() const { return _size; }
	auto GetMemory() const { return _memory; }
	auto GetProperties() const { return _properties; }
	bool IsDeviceLocal() const { return (_properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0; }

	void Bind(const std::vector<Buffer> &buffers);
	void Bind(ImageResource *image);
};

// Buffer
class BufferResource
{
private:
	VkBuffer _buffer = VK_NULL_HANDLE;

	VkDeviceSize _size = 0; // Size of the buffer itself specified during creation, not the amount of memory required to allocate the buffer.

	Memory _memory = nullptr;
	VkDeviceSize _offsetWithinMemory = 0;

	void *_mappedMemory = nullptr;
	Buffer _stagingBuffer = nullptr;

public:
	BufferResource(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage);
	~BufferResource();

	void CopyFrom(const void *source, VkDeviceSize copyOffset = 0, VkDeviceSize copySize = VK_WHOLE_SIZE);
	void CopyFrom(const Buffer &source, VkDeviceSize copyOffset = 0, VkDeviceSize copySize = VK_WHOLE_SIZE);

	auto Size() const { return _size; }
	auto GetBuffer() const { return _buffer; }
	auto GetMemory() const { return _memory; }
	void SetMemory(const Memory &memory, VkDeviceSize offset);
};

class ImageResource
{
private:
	VkImage _image = VK_NULL_HANDLE;
	VkImageView _imageView = VK_NULL_HANDLE;

	uint32_t _width = 0;
	uint32_t _height = 0;

	Memory _memory = nullptr;

	void *_mappedMemory = nullptr;
	Buffer _stagingBuffer = nullptr;

	bool _isSwapChainImage = false;

public:
	ImageResource(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkImageAspectFlags aspectFlags);
	ImageResource(VkImage image, VkImageView imageView) : _image(image), _imageView(imageView) { _isSwapChainImage = true; }
	~ImageResource();

	void CopyFrom(const void *source, uint32_t width, uint32_t height);
	void CopyFrom(const Buffer &buffer, uint32_t width, uint32_t height);

	auto Size() const { return _width * _height * 4; } // Four bytes per pixel
	auto GetImage() const { return _image; }
	auto GetImageView() const { return _imageView; }
	auto GetMemory() const { return _memory; }
	void SetMemory(const Memory &memory);
};




