#include "HashGrid.h"

const size_t HashGrid::MAX_ADJACENT_GRID = 8;
const float HashGrid::KERNEL_RADIUS = 1.0f;

HashGrid::HashGrid(const std::vector<Particle> &particles, const float &particleMass, const glm::uvec3 resolution, float gridSpacing) :
	_particles(particles),
	_particleMass(particleMass),
	_resolution(resolution),
	_gridSpacing(gridSpacing)
{
	_buckets.resize(_resolution.x * _resolution.y * _resolution.z);
	for (size_t i = 0; i < _particles.size(); ++i)
	{
		size_t key = PositionToHashKey(_particles[i]._position);
		_buckets[key].push_back(i); // Store the index of the particle
	}

	_densities.resize(_particles.size());
}

glm::uvec3 HashGrid::PositionToBucketIndex(glm::vec3 position) const
{
	glm::uvec3 bucketIndex
	{
		static_cast<uint32_t>(std::floor(position.x / _gridSpacing)),
		static_cast<uint32_t>(std::floor(position.y / _gridSpacing)),
		static_cast<uint32_t>(std::floor(position.z / _gridSpacing))
	};

	return bucketIndex;
}

size_t HashGrid::BucketIndexToHashKey(glm::uvec3 bucketIndex) const
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

void HashGrid::ForEachNearbyParticles(const Particle &particle, float radius, const std::function<void(size_t, const Particle &)> &callback) const
{
	std::vector<size_t> adjacentKeys = GetAdjacentKeys(particle._position); // Total eight adjacent grids in the 3D space
	for (size_t i = 0; i < MAX_ADJACENT_GRID; ++i)
	{
		const auto &bucket = _buckets[adjacentKeys[i]];
		size_t particleCountInBucket = bucket.size();

		for (size_t j = 0; j < particleCountInBucket; ++j)
		{
			size_t particleIndex = bucket[j];
			float distance = glm::distance(_particles[particleIndex]._position, particle._position);
			if (distance < radius)
			{
				callback(particleIndex, _particles[particleIndex]);
			}
		}
	}
}

std::vector<size_t> HashGrid::GetAdjacentKeys(glm::vec3 position) const
{
	glm::vec3 originIndex = PositionToBucketIndex(position);
	std::vector<glm::vec3> adjacentBucketIndices(MAX_ADJACENT_GRID);

	for (size_t i = 0; i < MAX_ADJACENT_GRID; ++i)
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

	std::vector<size_t> adjacentKeys(MAX_ADJACENT_GRID);
	for (size_t i = 0; i < MAX_ADJACENT_GRID; ++i)
	{
		adjacentKeys[i] = BucketIndexToHashKey(adjacentBucketIndices[i]);
	}

	return adjacentKeys;
}

void HashGrid::UpdateDensities()
{
	Kernel kernel(KERNEL_RADIUS);

	size_t particleCount = _densities.size();

	#pragma omp parallel for
	for (int i = 0; i < particleCount; ++i)
	{
		float sum = 0.0f;
		ForEachNearbyParticles
		(
			_particles[i],
			KERNEL_RADIUS,
			[&](size_t j, const Particle &neighborParticle)
			{
				float distance = glm::distance(_particles[i]._position, neighborParticle._position);
				sum += kernel.GetValue(distance);
			}
		);

		_densities[i] = sum * _particleMass;
	}
}

glm::vec3 HashGrid::Interpolate(const Particle &particle, const std::vector<glm::vec3> &values) const
{
	assert(values.size() == _particles.size());

	glm::vec3 sum{};
	Kernel kernel(KERNEL_RADIUS);

	ForEachNearbyParticles
	(
		particle,
		KERNEL_RADIUS,
		[&](size_t j, const Particle &neighborParticle)
		{
			float distance = glm::distance(particle._position, neighborParticle._position);
			float weight = _particleMass / _densities[j] * kernel.GetValue(distance);
			sum += weight * values[j];
		}
	);
}

glm::vec3 HashGrid::GradientAt(size_t i, const Particle &particle, const std::vector<glm::vec3> &values) const
{
	assert(values.size() == _particles.size());

	// Temp - use neighbor list instead...?
	glm::vec3 sum{};
	Kernel kernel(KERNEL_RADIUS);

	ForEachNearbyParticles
	(
		particle,
		KERNEL_RADIUS,
		[&](size_t j, const Particle &neighborParticle)
		{
			auto neighborPosition = neighborParticle._position;
			float distance = glm::distance(particle._position, neighborPosition);
			if (distance > 0.0f)
			{
				auto direction = (neighborPosition - particle._position) / distance;
				sum += _densities[i] * _particleMass * (values[i] / (_densities[i] * _densities[i]) + values[i] / (_densities[j] * _densities[j])) * kernel.Gradient(distance, direction);
			}
		}
	);

	return sum;
}

float HashGrid::LaplacianAt(size_t i, const Particle &particle, const std::vector<float> &values) const
{
	assert(values.size() == _particles.size());

	// Temp - use neighbor list instead...?
	float sum = 0.0f;
	Kernel kernel(KERNEL_RADIUS);

	ForEachNearbyParticles
	(
		particle,
		KERNEL_RADIUS,
		[&](size_t j, const Particle &neighborParticle)
		{
			auto neighborPosition = neighborParticle._position;
			float distance = glm::distance(particle._position, neighborPosition);
			if (distance > 0.0f)
			{
				sum += _particleMass * kernel.SecondDerivative(distance) * (values[j] - values[i]) / _densities[j];
			}
		}
	);

	return sum;
}