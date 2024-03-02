#pragma once

#include <omp.h>

#include "VulkanCore.h"
#include "HashGrid.h"
#include "BVH.h"
#include "SimulationParameters.h"
#include "Delegate.h"

#include "MeshModel.h"
#include "Billboards.h"
#include "MarchingCubes.h"

enum class ColliderRenderMode
{
	Solid,
	Invisible,
	Wireframe
};

enum class ParticleRenderingMode
{
	Billboards,
	MarchingCubes
};

class SimulatedScene
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

	bool _isPlaying = false;
	Delegate<void(bool)> _onSetPlay;

	// Particles in the data structure
	size_t _particleCount = 0;
	
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

	// Physics parameters
	static const glm::vec3 GRAVITY;
	std::unique_ptr<SimulationParameters> _simulationParameters = std::make_unique<SimulationParameters>();

	ParticleRenderingMode _particleRenderingMode = ParticleRenderingMode::MarchingCubes;

	// Rendering
	std::unique_ptr<Billboards> _billboards = nullptr;
	std::unique_ptr<MarchingCubes> _marchingCubes = nullptr;

	Delegate<void(ParticleRenderingMode)> _onSetParticleRenderingMode;

	// Prop
	std::vector<std::unique_ptr<MeshModel>> _propModels;

public:
	SimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	auto &GetSimulationParameters() { return *_simulationParameters; }
	void SetPlay(bool play);
	bool IsPlaying() { return _isPlaying; }

	void SetParticleRenderingMode(ParticleRenderingMode particleRenderingMode);

	void InitializeParticles(float particleRadius, float distanceBetweenParticles, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange);
	void AddProp(const std::string &OBJPath, const std::string &texturePath = "", bool isVisible = true, bool isCollidable = true);

	MarchingCubes *GetMarchingCubes() { return _marchingCubes.get(); }

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
	void ApplyRenderMode(ParticleRenderingMode particleRenderingMode, bool play);
	void ApplyParticlePositions();
};