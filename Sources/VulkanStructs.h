#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Buffer
{
	VkBuffer _buffer = VK_NULL_HANDLE;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
};

struct Image
{
	VkImage _image = VK_NULL_HANDLE;
	VkDeviceMemory _imageMemory = VK_NULL_HANDLE;
	VkImageView _imageView = VK_NULL_HANDLE;
};

