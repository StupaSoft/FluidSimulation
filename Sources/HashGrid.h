#pragma once

#include <vector>
#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Kernel.h"

class HashGrid
{
private:
	float _gridSpacing;
	glm::ivec3 _resolution = glm::vec3(1.0f, 1.0f, 1.0f);
	std::vector<std::vector<uint32_t>> _buckets;
	std::vector<std::vector<uint32_t>> _neighbors;

	static const size_t OVERLAPPING_BUCKETS = 8;

public:
	HashGrid(size_t particleCount, glm::ivec3 resolution);
	void UpdateGrid(const std::vector<glm::vec3> &positions);
	void UpdateSpacing(float gridSpacing);
	void ForEachNeighborParticle(const std::vector<glm::vec3> &positions, size_t particleIndex, const std::function<void(size_t)> &callback) const;

private:
	// Position -> Bucket index -> Hash key
	// Input position to integer coordinate that corresponds to the bucket at grid cell (x, y, z)
	glm::ivec3 PositionToBucketIndex(glm::vec3 position) const;
	size_t BucketIndexToHashKey(glm::ivec3 bucketIndex) const;
	size_t PositionToHashKey(glm::vec3 position) const;

	std::vector<size_t> GetAdjacentKeys(glm::vec3 position) const;
};

