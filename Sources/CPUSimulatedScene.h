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
	std::vector<glm::vec3> _pressureForces;

	std::vector<glm::vec3> _nextPositions;
	std::vector<glm::vec3> _nextVelocities;

	glm::uvec3 _gridResolution = glm::uvec3(100, 100, 100);
	std::unique_ptr<HashGrid> _hashGrid = nullptr;
	std::unique_ptr<BVH> _bvh = std::make_unique<BVH>();

	Kernel _kernel = Kernel(0.0f);

public:
	CPUSimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore);

	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) override;
	virtual void AddProp(const std::string &OBJPath, const std::string &texturePath = "", bool isVisible = true, bool isCollidable = true) override;

	virtual void Update(float deltaSecond) override;
	
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