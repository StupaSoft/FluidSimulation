#pragma once

#include <omp.h>

#include "VulkanCore.h"
#include "HashGrid.h"
#include "BVH.h"
#include "SimulationParameters.h"
#include "Delegate.h"

#include "SimulatedSceneBase.h"

class CPUSimulatedScene : public SimulatedSceneBase
{
private:
	std::vector<glm::vec3> _positions;
	std::vector<glm::vec3> _velocities;
	std::vector<glm::vec3> _forces;
	std::vector<float> _densities;
	std::vector<float> _pressures;

	std::vector<glm::vec3> _nextPositions;
	std::vector<glm::vec3> _nextVelocities;

	std::unique_ptr<HashGrid> _hashGrid = nullptr;
	std::unique_ptr<Kernel> _kernel = nullptr;

	size_t _particleCount = 0;

	std::vector<Buffer> _particlePositionInputBuffers;

	bool _isPlaying = false;

public:
	virtual void Register() override;

	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) override;

private:
	void Update();

	void BeginTimeStep();
	void EndTimeStep();

	void AccumulateForces();
	void AccumulateExternalForce();
	void AccumulateViscosityForce();
	void AccumulatePressureForce();
	void ResolveCollision();

	void TimeIntegration(float deltaSecond);

	glm::vec3 GetWindVelocityAt(glm::vec3 samplePosition);
	// Compute the pressure from the equation-of-state
	float ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent);

	void UpdateDensities();

	// Reflect the particle status to the render system
	void Applypositions();
};