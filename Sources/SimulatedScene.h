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

struct SimulationParameters
{
	float _particleMass = 0.05f;

	float _targetDensity = 160.0f;
	float _soundSpeed = 2.0f;
	float _eosExponent = 2.0f;
	float _kernelRadiusFactor = 4.0f;

	float _dragCoefficient = 0.001f;
	float _viscosityCoefficient = 0.001f;
	float _restitutionCoefficient = 0.5f;
	float _frictionCoefficient = 0.5f;
};

class SimulatedScene
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

	bool _play = false;

	// Particles in the data structure
	size_t _particleCount = 0;
	float _particleRadius = 0.03f;

	std::shared_ptr<MeshModel> _particleModel = nullptr;
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

	glm::uvec3 _gridResolution = glm::uvec3(100, 100, 100);
	std::unique_ptr<HashGrid> _hashGrid = nullptr;
	std::unique_ptr<BVH> _bvh = std::make_unique<BVH>();

	Kernel _kernel = Kernel(0.0f);

	// 3 2
	// 0 1
	const std::vector<glm::vec2> VERTICES_IN_PARTICLE{ {-1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } }; // (Camera right component, camera up component, 0.0f)
	const std::vector<uint32_t> INDICES_IN_PARTICLE{ 0, 1, 2, 0, 2, 3 };

	// Physics parameters
	static const glm::vec3 GRAVITY;
	SimulationParameters simulationParameters{};

public:
	SimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	auto &GetSimulationParameters() { return simulationParameters; }
	void SetPlay(bool play) { _play = play; }
	bool IsPlaying() { return _play; }

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

	glm::vec3 GetWindVelocityAt(glm::vec3 samplePosition);
	// Compute the pressure from the equation-of-state
	float ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent);

	void UpdateDensities();

	// Reflect the particle status to the render system
	void ApplyParticlePositions();
};