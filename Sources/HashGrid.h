#pragma once

#include <vector>
#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Particle.h"
#include "Kernel.h"

class HashGrid
{
private:
	const std::vector<Particle> &_particles;

	float _gridSpacing = 1.0f; // Size of a grid
	glm::uvec3 _resolution = glm::vec3(1.0f, 1.0f, 1.0f);
	std::vector<std::vector<uint32_t>> _buckets;

	std::vector<float> _densities;

	static const size_t MAX_ADJACENT_GRID;
	static const float KERNEL_RADIUS;
	const float &_particleMass;

public:
	HashGrid(const std::vector<Particle> &particles, const float &particleMass, const glm::uvec3 resolution, float gridSpacing);

private:
	// Position -> Bucket index -> Hash key
	// Input position to integer coordinate that corresponds to the bucket at grid cell (x, y, z)
	glm::uvec3 PositionToBucketIndex(glm::vec3 position) const;
	size_t BucketIndexToHashKey(glm::uvec3 bucketIndex) const;
	size_t PositionToHashKey(glm::vec3 position) const;

	void ForEachNearbyParticles(const Particle &particle, float radius, const std::function<void(size_t, const Particle &)> &callback) const;
	std::vector<size_t> GetAdjacentKeys(glm::vec3 position) const;

	void UpdateDensities();

	// Interpolate the values at the given particle position.
	glm::vec3 Interpolate(const Particle &particle, const std::vector<glm::vec3> &values) const;
	// Symmetric gradient
	glm::vec3 GradientAt(size_t i, const Particle &particle, const std::vector<glm::vec3> &values) const;
	float LaplacianAt(size_t i, const Particle &particle, const std::vector<float> &values) const;
};

