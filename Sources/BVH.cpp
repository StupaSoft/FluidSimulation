#include "BVH.h"

const glm::vec4 BVH::OFFSET = 1e-5f * glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

void BVH::AddPropObject(const std::shared_ptr<MeshObject> &propObject)
{
	_propObjects.push_back(propObject);
}

bool BVH::GetIntersection(glm::vec3 currentPosition, glm::vec3 nextPosition, Intersection *intersection)
{
	bool isHit = false;
	float minDistance = std::numeric_limits<float>::infinity();

	std::stack<size_t> nodeStack;
	if (RayBoxIntersection(_nodes[0]._boundingBox, currentPosition, nextPosition)) nodeStack.push(0); // Push the root
	while (!nodeStack.empty())
	{
		uint32_t nodeIndex = nodeStack.top();
		nodeStack.pop();

		auto node = _nodes[nodeIndex];
		if (!IsLeafNode(nodeIndex))
		{
			uint32_t child1 = node._child1;
			if (RayBoxIntersection(_nodes[child1]._boundingBox, currentPosition, nextPosition)) nodeStack.push(child1);
			
			uint32_t child2 = node._child2;
			if (RayBoxIntersection(_nodes[child2]._boundingBox, currentPosition, nextPosition)) nodeStack.push(child2);
		}
		else
		{
			Intersection propIntersection{};
			if (MollerTrumbore(node._boundingBox._triangle, currentPosition, nextPosition, &propIntersection))
			{
				float distance = glm::distance(currentPosition, propIntersection._point);
				if (distance < minDistance)
				{
					minDistance = distance;
					*intersection = std::move(propIntersection);
					isHit = true;
				}
			}
		}
	}

	return isHit;
}

// Top-down BVH tree construction with the surface area heuristic (SAH) cost.
bool BVH::Construct()
{
	// Calculate bounding boxes for all triangles
	std::vector<AABB> boundingBoxes;
	std::unordered_set<glm::vec3> centroidSet;
	for (const auto &propObject : _propObjects)
	{
		if (propObject->IsCollidable())
		{
			const auto &worldTriangles = propObject->GetWorldTriangles();
			for (const auto &triangle : worldTriangles)
			{
				// There should be no two bounding boxes with the same centroid.
				AABB boundingBox = TriangleToAABB(triangle);
				while (centroidSet.contains(Centroid(boundingBox)))
				{
					boundingBox._upperBound += OFFSET;
				}
				centroidSet.insert(Centroid(boundingBox));
				boundingBoxes.push_back(std::move(boundingBox));
			}
		}
	}

	// Handle an edge case
	if (boundingBoxes.empty())
	{
		return false;
	}

	// Initialize the root range
	AABB rootBB{};
	uint32_t count = 0;
	for (uint32_t i = 0; i < boundingBoxes.size(); ++i)
	{
		rootBB = Union(rootBB, boundingBoxes[i]);
	}

	Range rootRange
	{
		._start = 0,
		._end = static_cast<uint32_t>(boundingBoxes.size()),
		._parentIndex = NONE,
		._boundingBox = rootBB
	};

	// Contruct a hierarchy
	std::stack<Range> ranges;
	ranges.push(std::move(rootRange));
	uint32_t maxLevel = 1;
	while (!ranges.empty())
	{
		Range range = ranges.top();
		ranges.pop();

		// If the range spans to only one element, this one becomes the leaf node.
		if (range._end - range._start == 1)
		{
			uint32_t newLevel = range._parentIndex == NONE ? 1 : _nodes[range._parentIndex]._level + 1;
			if (newLevel > maxLevel) maxLevel = newLevel;
			Node leafNode
			{
				._boundingBox = boundingBoxes[range._start],
				._level = newLevel,
				._parent = range._parentIndex
			};

			_nodes.push_back(std::move(leafNode));

			uint32_t newNodeIndex = _nodes.size() - 1;
			if (_nodes[range._parentIndex]._child1 == NONE)
			{
				_nodes[range._parentIndex]._child1 = newNodeIndex;
			}
			else
			{
				_nodes[range._parentIndex]._child2 = newNodeIndex;
			}

			continue;
		}

		// Find the axis with the largest centroid stretch.
		AABB boundingBox = range._boundingBox;
		auto [targetAxis, minCentroid, maxCentroid] = GetTargetAxisAndMinMaxCentroid(boundingBoxes, range._start, range._end);

		// Decide a splitting plane that minimizes the SAH cost
		// We have to classify elements into buckets.
		static const uint32_t BUCKET_COUNT = 16;
		std::vector<Bucket> buckets(BUCKET_COUNT, Bucket{});
		float bucketInterval = (maxCentroid[targetAxis] - minCentroid[targetAxis]) / BUCKET_COUNT;
		for (uint32_t i = range._start; i < range._end; ++i)
		{
			float offsetFromStart = Centroid(boundingBoxes[i])[targetAxis] - minCentroid[targetAxis];
			uint32_t bucketIndex = static_cast<uint32_t>(offsetFromStart / bucketInterval);
			if (bucketIndex == BUCKET_COUNT) --bucketIndex;

			++buckets[bucketIndex]._count;
			buckets[bucketIndex]._bound = Union(buckets[bucketIndex]._bound, boundingBoxes[i]);
		}

		// Estimate costs for splitting into two groups
		std::vector<float> costs(BUCKET_COUNT - 1);
		std::vector<AABB> leftBBs(BUCKET_COUNT - 1); // Bounding boxes for the left child range
		std::vector<AABB> rightBBs(BUCKET_COUNT - 1); // Bounding boxes for the right child range
		for (uint32_t i = 0; i < BUCKET_COUNT - 1; ++i)
		{
			AABB leftBB{};
			uint32_t leftCount = 0;
			for (uint32_t j = 0; j <= i; ++j)
			{
				leftBB = Union(leftBB, buckets[j]._bound);
				leftCount += buckets[j]._count;
			}

			AABB rightBB{};
			uint32_t rightCount = 0;
			for (uint32_t j = i + 1; j < BUCKET_COUNT; ++j)
			{
				rightBB = Union(rightBB, buckets[j]._bound);
				rightCount += buckets[j]._count;
			}

			costs[i] = (leftCount * SurfaceArea(leftBB) + rightCount * SurfaceArea(rightBB)) / SurfaceArea(boundingBoxes[i]);
			leftBBs[i] = std::move(leftBB);
			rightBBs[i] = std::move(rightBB);
		}

		// Find the minimum estimated cost
		float minCost = std::numeric_limits<float>::max();
		uint32_t minCostSplitBucket = -1;
		for (uint32_t i = 0; i < BUCKET_COUNT - 1; ++i)
		{
			if (costs[i] < minCost)
			{
				minCost = costs[i];
				minCostSplitBucket = i;
			}
		}

		// Partition the range into two groups
		// Partition the bounding boxes by the selected bucket
		auto separatorIter = std::partition
		(
			boundingBoxes.begin() + range._start, 
			boundingBoxes.begin() + range._end, 
			[this, targetAxis, minCentroid, bucketInterval, minCostSplitBucket](const AABB &a) 
			{ 
				return (static_cast<uint32_t>((Centroid(a)[targetAxis] - minCentroid[targetAxis]) / bucketInterval)) <= minCostSplitBucket;
			}
		); 

		// Create a new node and register
		uint32_t newLevel = range._parentIndex == NONE ? 1 : _nodes[range._parentIndex]._level + 1;
		if (newLevel > maxLevel) maxLevel = newLevel;
		Node newNode
		{
			._boundingBox = range._boundingBox,
			._level = newLevel,
			._parent = range._parentIndex,
			// child1 and child2 will later be set by children.
		};

		_nodes.push_back(std::move(newNode));
		uint32_t newNodeIndex = _nodes.size() - 1;
		if (range._parentIndex != NONE)
		{
			if (_nodes[range._parentIndex]._child1 == NONE)
			{
				_nodes[range._parentIndex]._child1 = newNodeIndex;
			}
			else
			{
				_nodes[range._parentIndex]._child2 = newNodeIndex;
			}
		}
		
		// Set and push the child nodes
		uint32_t childStart = range._start;
		uint32_t separator = separatorIter - boundingBoxes.begin();
		uint32_t childEnd = range._end;

		Range childRange1
		{
			._start = childStart,
			._end = separator,
			._parentIndex = newNodeIndex,
			._boundingBox = leftBBs[minCostSplitBucket]
		};

		Range childRange2
		{
			._start = separator,
			._end = childEnd,
			._parentIndex = newNodeIndex,
			._boundingBox = rightBBs[minCostSplitBucket]
		};

		ranges.push(childRange1);
		ranges.push(childRange2);
	}

	std::cout << "Maximum level: " << maxLevel << std::endl;

	return true;
}

inline bool BVH::IsLeafNode(uint32_t nodeIndex)
{
	return _nodes[nodeIndex]._child1 == NONE;
}

bool BVH::RayBoxIntersection(const AABB &boundingBox, glm::vec3 start, glm::vec3 end)
{
	float tMin = 0.0f;
	float tMax = std::numeric_limits<float>::infinity();

	glm::vec3 ray = end - start;
	for (size_t i = 0; i < 3; ++i)
	{
		float t1 = (boundingBox._lowerBound[i] - start[i]) / ray[i];
		float t2 = (boundingBox._upperBound[i] - start[i]) / ray[i];

		tMin = std::min(std::max(t1, tMin), std::max(t2, tMin));
		tMax = std::max(std::min(t1, tMax), std::min(t2, tMax));
	}

	return tMin <= tMax;
}

bool BVH::MollerTrumbore(const Triangle &triangle, glm::vec3 start, glm::vec3 end, Intersection *intersection)
{
	// Moller-Trumbore alogrithm
	constexpr float EPSILON = std::numeric_limits<float>::epsilon();

	glm::vec3 ray = end - start;

	glm::vec3 edge1 = triangle.B - triangle.A;
	glm::vec3 edge2 = triangle.C - triangle.A;
	glm::vec3 rayCrossEdge2 = glm::cross(ray, edge2);
	float det = glm::dot(edge1, rayCrossEdge2);

	if (det > -EPSILON && det < EPSILON) return false; // This ray is parallel to this triangle.

	float inverseDet = 1.0f / det;
	glm::vec3 s = start - triangle.A.xyz;
	float u = inverseDet * glm::dot(s, rayCrossEdge2);

	if (u < 0.0f || u > 1.0f) return false;

	glm::vec3 sCrossEdge1 = glm::cross(s, edge1);
	float v = inverseDet * glm::dot(ray, sCrossEdge1);

	if (v < 0.0f || u + v > 1.0f) return false;

	// At this stage we can compute t to find out where the intersection point is on the line.
	float t = inverseDet * dot(edge2, sCrossEdge1);
	if (0.0f <= t && t <= 1.0f)
	{
		intersection->_point = start + t * ray;
		intersection->_normal = glm::normalize((1.0f - u - v) * triangle.normalA + u * triangle.normalB + v * triangle.normalC);
		intersection->_pointVelocity = glm::vec3(); // Temp

		return true;
	}
	else
	{
		return false;
	}
}

auto BVH::TriangleToAABB(const Triangle &t) -> AABB
{
	AABB bb
	{
		._triangle = t,
		._lowerBound = glm::min(glm::min(t.A, t.B), t.C),
		._upperBound = glm::max(glm::max(t.A, t.B), t.C)
	};

	// We could add a small amount of offset to the lower bound and the upper bound to prevent flat AABBs.
	// Flat AABBs result in edge cases during the construction of a BVH.
	bb._lowerBound -= OFFSET;
	bb._upperBound += OFFSET;
	return bb;
}

auto BVH::Union(const AABB &a, const AABB &b) -> AABB
{
	AABB unioned
	{
		._lowerBound = glm::min(a._lowerBound, b._lowerBound),
		._upperBound = glm::max(a._upperBound, b._upperBound)
	};

	return unioned;
}

glm::vec3 BVH::Centroid(const AABB &a)
{
	return (a._lowerBound + a._upperBound) / 2.0f;
}

float BVH::SurfaceArea(const AABB &a)
{
	glm::vec3 d = a._upperBound - a._lowerBound;
	return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
}

std::tuple<uint32_t, glm::vec3, glm::vec3> BVH::GetTargetAxisAndMinMaxCentroid(const std::vector<AABB> &boundingBoxes, uint32_t start, uint32_t end)
{
	// Get all the centroids in the range
	std::vector<glm::vec3> centroids;
	centroids.reserve(end - start);
	for (uint32_t i = start; i < end; ++i)
	{
		centroids.push_back(Centroid(boundingBoxes[i]));
	}

	// Find the target axis with the maximum centroid range
	uint32_t targetAxis = 0;
	float maxCentroidStretch = std::numeric_limits<float>::lowest();
	glm::vec3 minCentroid = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	glm::vec3 maxCentroid = glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
	
	const uint32_t NUM_AXES = 3;
	for (uint32_t i = 0; i < NUM_AXES; ++i)
	{
		glm::vec3 minCentroidForAxis = *std::min_element(centroids.begin(), centroids.end(), [i](glm::vec3 c1, glm::vec3 c2) { return c1[i] < c2[i]; });
		glm::vec3 maxCentroidForAxis = *std::max_element(centroids.begin(), centroids.end(), [i](glm::vec3 c1, glm::vec3 c2) { return c1[i] < c2[i]; });

		float centroidStretch = maxCentroidForAxis[i] - minCentroidForAxis[i];
		if (centroidStretch > maxCentroidStretch)
		{
			maxCentroidStretch = centroidStretch;
			targetAxis = i;
			minCentroid = minCentroidForAxis;
			maxCentroid = maxCentroidForAxis;
		}
	}

	return std::make_tuple(targetAxis, minCentroid, maxCentroid);
}

void BVH::DrawBoundingBoxes(const std::shared_ptr<VulkanCore> &vulkanCore, uint32_t nodeIndex, bool includeDescendants)
{
	if (_meshModel == nullptr)
	{
		_meshModel = MeshModel::Instantiate<MeshModel>(vulkanCore);

		// 2 3
		// 0 1
		// -----
		// 6 7
		// 4 5
		std::vector<Vertex> boxVertices;
		Vertex vertex{};

		vertex.pos = glm::vec3(-0.5f, -0.5f, -0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(0.5f, -0.5f, -0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(-0.5f, 0.5f, -0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(0.5f, 0.5f, -0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(-0.5f, -0.5f, 0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(0.5f, -0.5f, 0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(-0.5f, 0.5f, 0.5f);
		boxVertices.push_back(vertex);
		vertex.pos = glm::vec3(0.5f, 0.5f, 0.5f);
		boxVertices.push_back(vertex);

		std::vector<uint32_t> boxIndices
		{
			0, 1,
			0, 2,
			0, 4,
			1, 3,
			1, 5,
			2, 3,
			2, 6,
			3, 7,
			4, 5,
			4, 6,
			5, 7,
			6, 7
		};

		_meshModel->LoadMesh(boxVertices, boxIndices);
		_meshModel->SetLineWidth(2.0f);
		_meshModel->LoadPipeline("Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv", RenderMode::Line);
	}

	if (includeDescendants)
	{
		std::stack<size_t> nodeStack;
		nodeStack.push(nodeIndex);
		while (!nodeStack.empty())
		{
			nodeIndex = nodeStack.top();
			nodeStack.pop();

			AddBoundingBoxToModel(nodeIndex, _meshModel.get());

			if (!IsLeafNode(nodeIndex))
			{
				uint32_t child1 = _nodes[nodeIndex]._child1;
				nodeStack.push(child1);
				uint32_t child2 = _nodes[nodeIndex]._child2;
				nodeStack.push(child2);
			}
		}
	}
	else
	{
		AddBoundingBoxToModel(nodeIndex, _meshModel.get());
	}
}

void BVH::AddBoundingBoxToModel(uint32_t nodeIndex, MeshModel *meshModel)
{
	const auto &boundingBox = _nodes[nodeIndex]._boundingBox;
	glm::vec3 boxStretch = boundingBox._upperBound - boundingBox._lowerBound;

	auto boundingBoxObject = meshModel->AddMeshObject();
	boundingBoxObject->SetScale(boxStretch);
	boundingBoxObject->SetPosition(Centroid(boundingBox));
}
