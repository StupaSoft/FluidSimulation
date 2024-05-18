#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanCore.h"
#include "VulkanUtility.h"
#include "VulkanResources.h"
#include "Camera.h"
#include "Triangle.h"

class MeshObject : public DelegateRegistrable<MeshObject>
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
	bool _isVisible = true;
	bool _isCollidable = true;

	// ==================== Vulkan resources ====================
	std::vector<Buffer> _mvpBuffers; // Create multiple buffers for each frame

	// ==================== Transform ====================
	glm::mat4 _translation = glm::mat4(1.0f);
	glm::mat4 _rotation = glm::mat4(1.0f);
	glm::mat4 _scale = glm::mat4(1.0f);

	// ==================== Triangle ====================
	std::shared_ptr<std::vector<Triangle>> _triangles; // Triangles in the model space
	std::vector<Triangle> _worldTriangles; // Triangles in the world space

public:
	explicit MeshObject(const std::shared_ptr<std::vector<Triangle>> &triangles);
	MeshObject(const MeshObject &other) = delete;
	MeshObject(MeshObject &&other) = default;
	MeshObject &operator=(const MeshObject &other) = delete;
	MeshObject &operator=(MeshObject &&other) = default;
	virtual ~MeshObject() = default; 
	
	virtual void Register() override;

	std::vector<Buffer> GetMVPBuffers();
	void CleanUp();

	const std::vector<Triangle> &GetWorldTriangles() const { return _worldTriangles; }

	void SetVisible(bool visible) { _isVisible = visible; }
	bool IsVisible() { return _isVisible; }
	void SetCollidable(bool collidable) { _isCollidable = collidable; }
	bool IsCollidable() { return _isCollidable; }

	void SetPosition(glm::vec3 position);
	void SetRotation(glm::vec3 rotation);
	void SetScale(glm::vec3 scale);

	void Translate(glm::vec3 offset);
	void Rotate(glm::vec3 axis, float angle);
	void Scale(glm::vec3 scale);

private:
	void ApplyModelTransformation();
	void SetCameraTransformation(const glm::mat4 &view, const glm::mat4 &projection);
	void UpdateWorldTriangles(const glm::mat4 &model);
};