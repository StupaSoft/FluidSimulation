#pragma once

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Particle
{
	glm::vec3 _position{};
	glm::vec3 _velocity{};
	glm::vec3 _force{};

	float _density = 0.0f;
	float _pressure = 0.0f;
	glm::vec3 _pressureForce{};
};

