#pragma once

#include <vector>
#include <memory>
#include <stack>
#include <unordered_set>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "MeshModel.h"
#include "Triangle.h"
#include "MathUtil.h"

struct Intersection
{
	glm::vec3 _point{};
	glm::vec3 _normal{};
	glm::vec3 _pointVelocity{};
};

class BVH
{
public:
	static const uint32_t NONE = std::numeric_limits<uint32_t>::max();

	struct AABB
	{
		Triangle _triangle{};
		alignas(16) glm::vec4 _lowerBound = glm::vec4(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0);
		alignas(16) glm::vec4 _upperBound = glm::vec4(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), 0);
	};

	struct Node
	{
		AABB _boundingBox{};
		alignas(4) uint32_t _level = 1;
		alignas(4) uint32_t _parent = static_cast<uint32_t>(NONE);
		alignas(4) uint32_t _child1 = static_cast<uint32_t>(NONE); // -1 if this node is a leaf
		alignas(4) uint32_t _child2 = static_cast<uint32_t>(NONE);
	};

private:
	// Used for recursively building a tree
	struct Range
	{
		uint32_t _start = NONE;
		uint32_t _end = NONE; // Exclusive end
		uint32_t _parentIndex = NONE;
		AABB _boundingBox{};
	};

	struct Bucket
	{
		uint32_t _count = 0;
		AABB _bound{};
	};

private:
	std::vector<std::shared_ptr<MeshObject>> _propObjects;
	std::vector<Node> _nodes;

	std::shared_ptr<MeshModel> _meshModel = nullptr;

	static const glm::vec4 OFFSET;

public:
	void AddPropObject(const std::shared_ptr<MeshObject> &propObject);
	bool Construct();
	bool GetIntersection(glm::vec3 currentPosition, glm::vec3 nextPosition, Intersection *intersection);

	void DrawBoundingBoxes(const std::shared_ptr<VulkanCore> &vulkanCore, uint32_t nodeIndex, bool includeDescendants);

	auto &GetNodes() { return _nodes; }

private:
	bool RayBoxIntersection(const AABB &boundingBox, glm::vec3 start, glm::vec3 end);
	bool MollerTrumbore(const Triangle &triangle, glm::vec3 start, glm::vec3 end, Intersection *intersection);

	// Functions for building a tree
	auto TriangleToAABB(const Triangle &t) -> AABB;
	auto Union(const AABB &a, const AABB &b)->AABB;
	float SurfaceArea(const AABB &a);
	glm::vec3 Centroid(const AABB &a);
	uint32_t GetTargetAxis(const std::vector<AABB> &boundingBoxes, uint32_t start, uint32_t end);
	bool IsLeafNode(uint32_t nodeIndex);
	std::tuple<uint32_t, glm::vec3, glm::vec3> GetTargetAxisAndMinMaxCentroid(const std::vector<AABB> &boundingBoxes, uint32_t start, uint32_t end);

	// Functions for debugging a tree
	void AddBoundingBoxToModel(uint32_t nodeIndex, MeshModel *meshModel);
};

