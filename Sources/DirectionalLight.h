#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Delegate.h"

class DirectionalLight
{
protected:
	glm::vec3 _direction;
	glm::vec3 _color;
	float _intensity;

	Delegate<void(const DirectionalLight &)> _onChanged;

public:
	DirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity) : _direction(direction), _color(color), _intensity(intensity)  {}

	auto GetDirection() const { return _direction; }
	auto GetColor() const { return _color; }
	auto GetIntensity() const { return _intensity; }

	void SetDirection(glm::vec3 direction);
	void SetColor(glm::vec3 color);
	void SetIntensity(float intensity);

	Delegate<void(const DirectionalLight &)> &OnChanged() { return _onChanged; }
};

