#pragma once

#include <omp.h>

#include "VulkanCore.h"
#include "MeshModel.h"
#include "Particle.h"
#include "HashGrid.h"

class SimulatedScene
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore;

	std::vector<std::shared_ptr<MeshModel>> _levelModels;

	// Particles in the data structure
	size_t _particleCount;

	std::shared_ptr<MeshModel> _particleModel;
	std::vector<Particle> _particles;
	std::vector<Vertex> _particleVertices;
	std::vector<uint32_t> _particleIndices;
	std::vector<Particle> _nextParticles;

	std::unique_ptr<HashGrid> _hashGrid;

	Kernel _kernel = Kernel(0.0f);

	// 3 2
	// 0 1
	const std::vector<glm::vec2> VERTICES_IN_PARTICLE{ {-1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } }; // (Camera right component, camera up component, 0.0f)
	const std::vector<uint32_t> INDICES_IN_PARTICLE{ 0, 1, 2, 0, 2, 3 };

	// Physics invariants
	static const float DRAG_COEFF;
	static const glm::vec3 GRAVITY;
	float _particleMass;
	float _targetDensity;
	static const float SOUND_SPEED;
	static const float EOS_EXPONENT;
	static const glm::uvec3 GRID_RESOLUTION;
	float _kernelRadius;
	static const float VISCOSITY_COEFF;

public:
	SimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	void AddLevel(const std::string &OBJPath, const std::string &texturePath);
	void InitializeParticles(float particleMass, float particleRadius, float distanceBetweenParticles, float targetDensity, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange);

	void Update(float deltaSecond);
	
private:
	void BeginTimeStep();
	void EndTimeStep();

	void AccumulateForces(float deltaSecond);
	void AccumulateExternalForce(float deltaSecond);
	void AccumulateViscosityForce(float deltaSecond);
	void AccumulatePressureForce(float deltaSecond);

	void TimeIntegration(float deltaSecond);
	void ResolveCollision();

	// Reflect the particle status to the render system
	void ApplyParticlePositions();

	glm::vec3 GetWindVelocityAt(glm::vec3 samplePosition);
	// Compute the pressure from the equation-of-state
	void ComputePseudoViscosity();
	float ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent);

	void UpdateDensities();
	// Interpolate the values at the given particle position.
	glm::vec3 Interpolate(size_t particleIndex, const std::vector<glm::vec3> &values) const;
	// Symmetric gradient
	glm::vec3 GradientAt(size_t particleIndex, const std::vector<glm::vec3> &values) const;
	float LaplacianAt(size_t particleIndex, const std::vector<float> &values) const;
};