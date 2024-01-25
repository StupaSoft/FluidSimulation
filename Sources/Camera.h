#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
private:
	float _fovy; // Temp
	uint32_t _width = 0;
	uint32_t _height = 0;

public:
	Camera(float fovy, uint32_t width, uint32_t height) : _fovy(fovy), _width(width), _height(height) {}
	void SetFOV(float fovy);
	void SetExtent(uint32_t width, uint32_t height);

	glm::mat4 GetViewMatrix();
	glm::mat4 GetProjectionMatrix();
};

