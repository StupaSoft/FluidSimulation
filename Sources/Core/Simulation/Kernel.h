#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Kernel
{
private:
	float _r1;
	float _r2;
	float _r3;
	float _r4;
	float _r5;

public:
	explicit Kernel(float radius) :
		_r1(radius),
		_r2(radius * _r1),
		_r3(radius * _r2),
		_r4(radius * _r3),
		_r5(radius * _r4)

	{
	}

	float GetValue(float distance) const; // Smooth kernel
	float FirstDerivative(float distance) const; // Spiky kernel
	float SecondDerivative(float distance) const; // Spiky kernel
	glm::vec3 Gradient(float distance, glm::vec3 directionToCenter) const;
};

