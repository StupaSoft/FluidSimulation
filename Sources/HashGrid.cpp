#include "HashGrid.h"

const size_t HashGrid::OVERLAPPING_GRID = 8;

HashGrid::HashGrid(const std::vector<glm::vec3> &positions, glm::ivec3 resolution, float kernelRadius) :
	_positions(positions),
	_resolution(resolution),
	_gridSpacing(kernelRadius)
{
	_buckets.resize(_resolution.x * _resolution.y * _resolution.z);
	_neighbors.resize(_positions.size());
}

void HashGrid::UpdateGrid()
{
	// 1. Update the grids
	size_t particleCount = _positions.size();

	size_t bucketCount = _buckets.size();
	#pragma omp parallel for
	for (size_t i = 0; i < bucketCount; ++i)
	{
		_buckets[i].clear();
	}

	for (size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
	{
		size_t key = PositionToHashKey(_positions[particleIndex]);
		_buckets[key].push_back(particleIndex); // Store the index of the particle
	}

	// 2. Update the neighbor list
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
	{
		_neighbors[particleIndex].clear();

		// For each adjacent overlapping grid key
		std::vector<size_t> adjacentKeys = GetAdjacentKeys(_positions[particleIndex]);
		for (size_t i = 0; i < OVERLAPPING_GRID; ++i)
		{
			const auto &bucket = _buckets[adjacentKeys[i]];
			for (size_t neighborIndex : bucket)
			{
				if (particleIndex != neighborIndex) _neighbors[particleIndex].push_back(neighborIndex);
			}
		}
	}
}

glm::ivec3 HashGrid::PositionToBucketIndex(glm::vec3 position) const
{
	glm::ivec3 bucketIndex
	{
		static_cast<uint32_t>(std::floor(position.x / _gridSpacing)),
		static_cast<uint32_t>(std::floor(position.y / _gridSpacing)),
		static_cast<uint32_t>(std::floor(position.z / _gridSpacing))
	};

	return bucketIndex;
}

size_t HashGrid::BucketIndexToHashKey(glm::ivec3 bucketIndex) const
{
	auto wrappedIndex = bucketIndex;
	wrappedIndex.x = bucketIndex.x % _resolution.x;
	wrappedIndex.y = bucketIndex.y % _resolution.y;
	wrappedIndex.z = bucketIndex.z % _resolution.z;
	if (wrappedIndex.x < 0) wrappedIndex.x += _resolution.x;
	if (wrappedIndex.y < 0) wrappedIndex.y += _resolution.y;
	if (wrappedIndex.z < 0) wrappedIndex.z += _resolution.z;

	return static_cast<size_t>((wrappedIndex.z * _resolution.y + wrappedIndex.y) * _resolution.x + wrappedIndex.x);
}

size_t HashGrid::PositionToHashKey(glm::vec3 position) const
{
	auto bucketIndex = PositionToBucketIndex(position);
	return BucketIndexToHashKey(bucketIndex);
}

void HashGrid::ForEachNeighborParticle(size_t particleIndex, const std::function<void(size_t)> &callback) const
{
	for (size_t neighborIndex : _neighbors[particleIndex])
	{
		float distance = glm::distance(_positions[particleIndex], _positions[neighborIndex]);
		if (distance < _gridSpacing)
		{
			callback(neighborIndex);
		}
	}
}

std::vector<size_t> HashGrid::GetAdjacentKeys(glm::vec3 position) const
{
	glm::ivec3 originIndex = PositionToBucketIndex(position);
	std::vector<glm::ivec3> adjacentBucketIndices(OVERLAPPING_GRID);

	for (size_t i = 0; i < OVERLAPPING_GRID; ++i)
	{
		adjacentBucketIndices[i] = originIndex;
	}

	// Get adjacent grids based on the position within the origin grid
	// (0, 0, 0) -> 0
	// ...
	// (0, 0, 1) -> 1
	// ...
	// (1, 1, 0) -> 6
	// ...
	// When we view the eight overlapping buckets
	// 3 7 // 2 6
	// 1 5 // 0 4
	if ((originIndex.x + 0.5f) * _gridSpacing <= position.x)
	{
		adjacentBucketIndices[4].x += 1;
		adjacentBucketIndices[5].x += 1;
		adjacentBucketIndices[6].x += 1;
		adjacentBucketIndices[7].x += 1;
	}
	else
	{
		adjacentBucketIndices[4].x -= 1;
		adjacentBucketIndices[5].x -= 1;
		adjacentBucketIndices[6].x -= 1;
		adjacentBucketIndices[7].x -= 1;
	}

	if ((originIndex.y + 0.5f) * _gridSpacing <= position.y)
	{
		adjacentBucketIndices[2].y += 1;
		adjacentBucketIndices[3].y += 1;
		adjacentBucketIndices[6].y += 1;
		adjacentBucketIndices[7].y += 1;
	}
	else
	{
		adjacentBucketIndices[2].y -= 1;
		adjacentBucketIndices[3].y -= 1;
		adjacentBucketIndices[6].y -= 1;
		adjacentBucketIndices[7].y -= 1;
	}

	if ((originIndex.z + 0.5f) * _gridSpacing <= position.z)
	{
		adjacentBucketIndices[1].z += 1;
		adjacentBucketIndices[3].z += 1;
		adjacentBucketIndices[5].z += 1;
		adjacentBucketIndices[7].z += 1;
	}
	else
	{
		adjacentBucketIndices[1].z -= 1;
		adjacentBucketIndices[3].z -= 1;
		adjacentBucketIndices[5].z -= 1;
		adjacentBucketIndices[7].z -= 1;
	}

	std::vector<size_t> adjacentKeys(OVERLAPPING_GRID);
	for (size_t i = 0; i < OVERLAPPING_GRID; ++i)
	{
		adjacentKeys[i] = BucketIndexToHashKey(adjacentBucketIndices[i]);
	}

	return adjacentKeys;
}