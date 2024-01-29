#pragma once

#include <omp.h>

#include "VulkanCore.h"
#include "MeshModel.h"
#include "Particle.h"

class SimulatedScene
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore;

	std::vector<std::shared_ptr<MeshModel>> _levelModels;

	// Particles in the data structure
	std::shared_ptr<MeshModel> _particleModel;
	std::vector<Particle> _particles;
	std::vector<Vertex> _particleVertices;
	std::vector<uint32_t> _particleIndices;

	std::vector<Particle> _nextParticles;

	// 3 2
	// 0 1
	const std::vector<glm::vec2> VERTICES_IN_PARTICLE{ {-1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } }; // (Camera right component, camera up component, 0.0f)
	const std::vector<uint32_t> INDICES_IN_PARTICLE{ 0, 1, 2, 0, 2, 3 };

	// Physics constants
	static const float DRAG_COEFF;
	static const glm::vec3 GRAVITY;
	float _particleMass = 0.1f;

public:
	SimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	void AddLevel(const std::string &OBJPath, const std::string &texturePath);
	void InitializeParticles(float particleMass, float particleRadius, float distanceBetweenParticles, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange);

	void Update(float deltaSecond);
	
private:
	void UpdateParticles(float deltaSecond);

	void BeginTimeStep();
	void EndTimeStep();

	void AccumulateForces(float deltaSecond);
	void TimeIntegration(float deltaSecond);
	void ResolveCollision();

	void ApplyParticlePositions();

	glm::vec3 GetWindVelocityAt(glm::vec3 samplePosition);
};