#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanUtility.h"
#include "VulkanCore.h"

// Descriptor used in shaders
// Use alignas to solve alignment issues
struct UniformBufferObject
{
	alignas(16) glm::mat4 model; // mat4 is binary-compatible with the shader's one
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

class MeshObject
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore;

	// ==================== Vulkan resources ====================
	std::vector<VkBuffer> _uniformBuffers; // Create multiple buffers for each frame
	std::vector<VkDeviceMemory> _uniformBuffersMemory;

	// ==================== Transform ====================
	glm::mat4 _position = glm::mat4(1.0f);
	glm::mat4 _rotation = glm::mat4(1.0f);

public:
	explicit MeshObject(const std::shared_ptr<VulkanCore> &vulkanCore);
	MeshObject(const MeshObject &other) = delete;
	MeshObject(MeshObject &&other) = default;
	MeshObject &operator=(const MeshObject &other) = delete;
	MeshObject &operator=(MeshObject &&other) = default;
	virtual ~MeshObject() = default;

	std::vector<VkBuffer> GetUniformBuffers();
	void CleanUp();

	void SetPosition(glm::vec3 position);
	void SetRotation(glm::vec3);
	void Translate(glm::vec3 offset);
	void Rotate(glm::vec3 axis, float angle);
	void ApplyTransformations();

private:
	std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> CreateUniformBuffers();
	std::vector<VkDescriptorSet> CreateDescriptorSets(const std::vector<VkBuffer> &uniformBuffers);
};