#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Buffer
struct BufferMemory;
using Buffer = std::shared_ptr<BufferMemory>;

struct BufferMemory
{
	VkDevice _logicalDevice = VK_NULL_HANDLE;

	VkBuffer _buffer = VK_NULL_HANDLE;
	VkDeviceMemory _memory = VK_NULL_HANDLE;

	VkDeviceSize _size = 0;
};

struct BufferDeleter 
{
	void operator()(BufferMemory *bufferMemory) const 
	{
		vkDestroyBuffer(bufferMemory->_logicalDevice, bufferMemory->_buffer, nullptr);
		vkFreeMemory(bufferMemory->_logicalDevice, bufferMemory->_memory, nullptr);
	}
};

// Image
struct ImageMemoryView;
using Image = std::shared_ptr<ImageMemoryView>;

struct ImageMemoryView
{
	VkDevice _logicalDevice = VK_NULL_HANDLE;

	VkImage _image = VK_NULL_HANDLE;
	VkDeviceMemory _imageMemory = VK_NULL_HANDLE;
	VkImageView _imageView = VK_NULL_HANDLE;
};

struct ImageDeleter
{
	void operator()(ImageMemoryView *imageMemoryView) const
	{
		vkDestroyImage(imageMemoryView->_logicalDevice, imageMemoryView->_image, nullptr);
		vkFreeMemory(imageMemoryView->_logicalDevice, imageMemoryView->_imageMemory, nullptr);
		vkDestroyImageView(imageMemoryView->_logicalDevice, imageMemoryView->_imageView, nullptr);
	}
};





