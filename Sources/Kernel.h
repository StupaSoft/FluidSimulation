#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Kernel
{
private:
	float _h1;
	float _h2;
	float _h3;
	float _h4;
	float _h5;

public:
	explicit Kernel(float radius) :
		_h1(radius),
		_h2(radius * _h1),
		_h3(radius * _h2),
		_h4(radius * _h3),
		_h5(radius * _h4)

	{
	}

	float GetValue(float distance) const; // Smooth kernel
	float FirstDerivative(float distance) const; // Spiky kernel
	float SecondDerivative(float distance) const; // Spiky kernel
	glm::vec3 Gradient(float distance, glm::vec3 directionToCenter) const;
};

