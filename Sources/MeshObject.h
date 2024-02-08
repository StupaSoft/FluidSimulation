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
#include "Camera.h"
#include "Triangle.h"

class MeshObject
{
public:
	// Descriptor used in shaders
	// Use alignas to solve alignment issues
	struct MVP
	{
		// MVP matrices
		alignas(16) glm::mat4 _model; // mat4 is binary-compatible with the shader's one
		alignas(16) glm::mat4 _view;
		alignas(16) glm::mat4 _projection;
	};

private:
	std::shared_ptr<VulkanCore> _vulkanCore;

	bool _isVisible = true;
	bool _isCollidable = true;

	// ==================== Vulkan resources ====================
	std::vector<VkBuffer> _mvpBuffers; // Create multiple buffers for each frame
	std::vector<VkDeviceMemory> _mvpBuffersMemory;

	// ==================== Transform ====================
	glm::mat4 _position = glm::mat4(1.0f);
	glm::mat4 _rotation = glm::mat4(1.0f);

	// ==================== Triangle ====================
	std::shared_ptr<std::vector<Triangle>> _triangles; // Triangles in the model space
	std::vector<Triangle> _worldTriangles; // Triangles in the world space

public:
	explicit MeshObject(const std::shared_ptr<VulkanCore> &vulkanCore, const std::shared_ptr<std::vector<Triangle>> &triangles);
	MeshObject(const MeshObject &other) = delete;
	MeshObject(MeshObject &&other) = default;
	MeshObject &operator=(const MeshObject &other) = delete;
	MeshObject &operator=(MeshObject &&other) = default;
	virtual ~MeshObject() = default;

	std::vector<VkBuffer> GetMVPBuffers();
	void CleanUp();

	const std::vector<Triangle> &GetWorldTriangles() const { return _worldTriangles; }

	void SetVisible(bool visible) { _isVisible = visible; }
	bool IsVisible() { return _isVisible; }
	void SetCollidable(bool collidable) { _isCollidable = collidable; }
	bool IsCollidable() { return _isCollidable; }

	void SetPosition(glm::vec3 position);
	void SetRotation(glm::vec3);
	void Translate(glm::vec3 offset);
	void Rotate(glm::vec3 axis, float angle);

private:
	void ApplyModelTransformation();
	void SetCameraTransformation(const glm::mat4 &view, const glm::mat4 &projection);
	void UpdateWorldTriangles(const glm::mat4 &model);
};