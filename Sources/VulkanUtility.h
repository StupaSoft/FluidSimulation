#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <string>
#include <format>
#include <optional>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Vertex.h"

// Find the relevant type of memory
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &requiredProperties);

// Return min(depth sample count, color sample count)
VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

std::tuple<VkBuffer, VkDeviceMemory> CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties);
void CopyBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
void CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

std::tuple<VkImage, VkDeviceMemory> CreateImageAndMemory(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties);
VkImageView CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

// Start a temporary command buffer
VkCommandBuffer BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
void EndSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue submitQueue);

VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char> &code);

std::vector<char> ReadFile(const std::string &fileName);
std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::string &OBJPath);

std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> CreateBuffersAndMemory(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, size_t objectSize, size_t maxFramesInFlight, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperty);
void CopyToBuffer(VkDevice logicalDevice, VkDeviceMemory bufferMemory, void *source, VkDeviceSize copyOffset, VkDeviceSize copySize);
void CopyToBuffers(VkDevice logicalDevice, const std::vector<VkDeviceMemory> &buffersMemory, void *source, VkDeviceSize copyOffset, VkDeviceSize copySize);

std::tuple<VkImage, VkDeviceMemory, VkImageView, uint32_t> CreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string &texturePath);
void GenerateMipmaps(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
