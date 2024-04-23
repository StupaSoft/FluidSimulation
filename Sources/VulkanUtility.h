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
#include "VulkanStructs.h"

// Find the relevant type of memory
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &requiredProperties);

// Return min(depth sample count, color sample count)
VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

Buffer CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties);
std::vector<Buffer> CreateBuffers(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, size_t bufferSize, size_t maxFramesInFlight, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperty);

void CopyBufferToBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, Buffer srcBuffer, Buffer dstBuffer, VkDeviceSize size);
void CopyMemoryToBuffer(VkDevice logicalDevice, const void *source, Buffer buffer, VkDeviceSize copyOffset, VkDeviceSize copySize = -1);
void CopyMemoryToBuffers(VkDevice logicalDevice, const void *source, const std::vector<Buffer> &buffers, VkDeviceSize copyOffset, VkDeviceSize copySize = -1);

Image CreateImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat imageFormat, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties, VkImageAspectFlags aspectFlags);
void CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, Buffer buffer, VkImage image, uint32_t width, uint32_t height);

// Start a temporary command buffer
VkCommandBuffer BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
void EndSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue submitQueue);

VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char> &code);

std::vector<char> ReadFile(const std::wstring &fileName);
std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::wstring &OBJPath);

std::tuple<Image, uint32_t> CreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::wstring &texturePath);
void GenerateMipmaps(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, Image image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout);
std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges);

size_t DivisionCeil(size_t x, size_t y);
