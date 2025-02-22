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
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Vertex.h"

// Find the relevant type of memory
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, const VkMemoryPropertyFlags &requiredProperties);

// Return min(depth sample count, color sample count)
VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char> &code);

std::vector<char> ReadFile(const std::string &fileName);
std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::string &OBJFileName);

std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout);
std::tuple<VkPipeline, VkPipelineLayout> CreateComputePipeline(VkDevice logicalDevice, VkShaderModule computeShaderModule, VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges);

uint32_t DivisionCeil(uint32_t x, uint32_t y);
