#pragma once

#include <tuple>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Delegate.h"

class Camera
{
private:
	glm::vec3 _position;
	glm::vec3 _direction;

	float _fovy; // Temp
	uint32_t _width = 0;
	uint32_t _height = 0;

	Delegate<void(const Camera &)> _onChanged;

public:
	Camera(glm::vec3 position, glm::vec3 direction, float fovy, uint32_t width, uint32_t height) : _position(position), _direction(direction), _fovy(fovy), _width(width), _height(height) {}

	auto GetPosition() const { return _position; }
	auto GetDirection(glm::vec3 direction) const { return _direction; }
	auto GetFOV(float fovy) const { return _fovy; };
	auto GetExtent() const { return std::make_tuple(_width, _height); };

	void SetPosition(glm::vec3 position);
	void SetDirection(glm::vec3 direction);
	void SetFOV(float fovy);
	void SetExtent(uint32_t width, uint32_t height);

	Delegate<void(const Camera &)> &OnChanged() { return _onChanged; }

	glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix() const;
};
