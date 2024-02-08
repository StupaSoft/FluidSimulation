#pragma once

#include <vector>
#include <memory>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Triangle.h"

class BVH
{
private:
	struct Intersection
	{
		glm::vec3 point{};
		glm::vec3 normal{};
		glm::vec3 pointVelocity{};
	};

public:
	void ResolveCollision(const std::vector<Triangle> &triangles, glm::vec3 currentPosition, glm::vec3 currentVelocity, float particleRadius, float restitutionCoefficient, float frictionCoefficient, glm::vec3 *newPosition, glm::vec3 *newVelocity);
	bool GetIntersection(const std::vector<Triangle> &triangles, glm::vec3 currentPosition, glm::vec3 newPosition, Intersection *intersection);
};

