#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Buffer
{
	VkBuffer _buffer;
	VkDeviceMemory _memory;
};

struct Image
{
	VkImage _image;
	VkDeviceMemory _imageMemory;
	VkImageView _imageView;
};

