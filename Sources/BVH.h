#pragma once

#include <vector>
#include <memory>

#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "MeshModel.h"
#include "Triangle.h"

struct Intersection
{
	glm::vec3 point{};
	glm::vec3 normal{};
	glm::vec3 pointVelocity{};
};

class BVH
{
private:
	std::vector<std::shared_ptr<MeshObject>> _propObjects;

public:
	void AddPropObject(const std::shared_ptr<MeshObject> &propObject);
	bool GetIntersection(glm::vec3 currentPosition, glm::vec3 nextPosition, Intersection *intersection);

private:
	bool MollerTrumbore(const Triangle triangle, glm::vec3 start, glm::vec3 end, Intersection *intersection);
};

