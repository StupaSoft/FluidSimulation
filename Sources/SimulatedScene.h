#pragma once

#include <omp.h>

#include "VulkanCore.h"
#include "MeshModel.h"
#include "HashGrid.h"
#include "BVH.h"

enum class ColliderRenderMode
{
	Solid,
	Invisible,
	Wireframe
};

class SimulatedScene
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore;

	// Particles in the data structure
	size_t _particleCount;
	float _particleRadius;

	std::shared_ptr<MeshModel> _particleModel;
	std::vector<Vertex> _particleVertices;
	std::vector<uint32_t> _particleIndices;

	std::vector<glm::vec3> _positions;
	std::vector<glm::vec3> _velocities;
	std::vector<glm::vec3> _forces;
	std::vector<float> _densities;
	std::vector<float> _pressures;
	std::vector<glm::vec3> _pressureForces;

	std::vector<glm::vec3> _nextPositions;
	std::vector<glm::vec3> _nextVelocities;

	std::unique_ptr<HashGrid> _hashGrid;
	std::unique_ptr<BVH> _bvh = std::make_unique<BVH>();

	Kernel _kernel = Kernel(0.0f);

	// 3 2
	// 0 1
	const std::vector<glm::vec2> VERTICES_IN_PARTICLE{ {-1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } }; // (Camera right component, camera up component, 0.0f)
	const std::vector<uint32_t> INDICES_IN_PARTICLE{ 0, 1, 2, 0, 2, 3 };

	// Physics invariants
	static const float DRAG_COEFF;
	static const glm::vec3 GRAVITY;
	float _particleMass = 0.1f;
	float _targetDensity = 100.0f;
	static const float SOUND_SPEED;
	static const float EOS_EXPONENT;
	static const glm::uvec3 GRID_RESOLUTION;
	float _kernelRadius;
	static const float VISCOSITY_COEFF;
	float _restitutionCoefficient = 0.5f;
	float _frictionCoefficient = 0.5f;

public:
	SimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	void InitializeParticles(float particleRadius, float distanceBetweenParticles, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange);
	void AddProp(const std::string &OBJPath, const std::string &texturePath = "", bool isVisible = true, bool isCollidable = true);

	void Update(float deltaSecond);
	
private:
	void BeginTimeStep();
	void EndTimeStep();

	void AccumulateForces(float deltaSecond);
	void AccumulateExternalForce(float deltaSecond);
	void AccumulateViscosityForce(float deltaSecond);
	void AccumulatePressureForce(float deltaSecond);
	void ResolveCollision();

	void TimeIntegration(float deltaSecond);

	// Reflect the particle status to the render system
	void ApplyParticlePositions();

	glm::vec3 GetWindVelocityAt(glm::vec3 samplePosition);
	// Compute the pressure from the equation-of-state
	float ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent);

	void UpdateDensities();
};