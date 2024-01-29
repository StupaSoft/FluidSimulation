#include "SimulatedScene.h"

const float SimulatedScene::DRAG_COEFF = 1e-4;
const glm::vec3 SimulatedScene::GRAVITY = glm::vec3(0.0f, -9.8f, 0.0f);

void SimulatedScene::AddLevel(const std::string &OBJPath, const std::string &texturePath)
{
	auto obj = LoadOBJ(OBJPath);

	auto levelModel = _vulkanCore->AddModel<MeshModel>();
	levelModel->LoadAssets(std::get<0>(obj), std::get<1>(obj), "Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv", texturePath);
	levelModel->AddMeshObject();

	_levelModels.emplace_back(std::move(levelModel));
}

void SimulatedScene::InitializeParticles(float particleMass, float particleRadius, float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	// Setup
	size_t xCount = std::ceil((xRange.g - xRange.r) / particleDistance);
	size_t yCount = std::ceil((yRange.g - yRange.r) / particleDistance);
	size_t zCount = std::ceil((zRange.g - zRange.r) / particleDistance);

	size_t totalCount = xCount * yCount * zCount;
	glm::vec3 startingPoint = glm::vec3(xRange.r, yRange.r, zRange.r);

	// Prepare data structures
	_particles.resize(totalCount);

	_particleVertices.clear();
	_particleVertices.resize(totalCount * VERTICES_IN_PARTICLE.size());

	_particleIndices.clear();
	_particleIndices.resize(totalCount * INDICES_IN_PARTICLE.size());

	// Initialize particles
	_particleMass = particleMass;
	for (size_t i = 0; i < totalCount; ++i)
	{
		// Set attributes within each particle
		for (size_t j = 0; j < VERTICES_IN_PARTICLE.size(); ++j)
		{
			_particleVertices[i * VERTICES_IN_PARTICLE.size() + j].normal.x = particleRadius;
			_particleVertices[i * VERTICES_IN_PARTICLE.size() + j].texCoord = VERTICES_IN_PARTICLE[j];
		}

		// Set global indices of triangles
		for (size_t j = 0; j < INDICES_IN_PARTICLE.size(); ++j)
		{
			// First particle: 0, 1, 2, 0, 2, 3
			// Second particle: 4, 5, 6, 4, 6, 7
			// ...
			_particleIndices[i * INDICES_IN_PARTICLE.size() + j] = static_cast<uint32_t>(i * VERTICES_IN_PARTICLE.size() + INDICES_IN_PARTICLE[j]);
		}
	}

	// Place particles
	for (size_t z = 0; z < zCount; ++z)
	{
		for (size_t y = 0; y < yCount; ++y)
		{
			for (size_t x = 0; x < xCount; ++x)
			{
				size_t particleIndex = z * (xCount * yCount) + y * xCount + x;
				_particles[particleIndex]._position = startingPoint + glm::vec3(x, y, z) * particleDistance;
			}
		}
	}

	for (size_t i = 0; i < totalCount; ++i)
	{
		auto &particle = _particles[i];
		particle._force = glm::vec3(rand() % 20, 40 + rand() % 10, rand() % 20); // Temp
	}

	// Make a copy of the particles that will be used in simulation
	_nextParticles = _particles;
	for (size_t i = 0; i < totalCount; ++i)
	{
		_nextParticles[i]._force = glm::vec3{};
	}

	// Create models
	_particleModel = _vulkanCore->AddModel<MeshModel>();
	_particleModel->LoadAssets(_particleVertices, _particleIndices, "Shaders/ParticleVertex.spv", "Shaders/ParticleFragment.spv"); // Temp
	_particleModel->AddMeshObject();

	ApplyParticlePositions();
}

void SimulatedScene::Update(float deltaSecond)
{
	UpdateParticles(deltaSecond);
}

void SimulatedScene::BeginTimeStep()
{

}

void SimulatedScene::EndTimeStep()
{
	// 1. Velocities and positions are applied
	// 2. Forces are set to zero
	_particles = _nextParticles;
}

void SimulatedScene::UpdateParticles(float deltaSecond)
{
	// Conduct simulation
	BeginTimeStep();

	AccumulateForces(deltaSecond);
	TimeIntegration(deltaSecond);
	ResolveCollision();

	EndTimeStep();

	// Render
	ApplyParticlePositions();
}

void SimulatedScene::AccumulateForces(float deltaSecond)
{
	size_t particleCount = _particles.size();

	#pragma omp parallel for
	for (int i = 0; i < particleCount; ++i) // Use int instead of size_t to support omp
	{
		auto &particle = _particles[i];

		// Apply gravity
		glm::vec3 force = _particleMass * GRAVITY;
		
		// Apply wind forces
		glm::vec3 relativeVelocity = particle._velocity - GetWindVelocityAt(particle._position);
		force += DRAG_COEFF * relativeVelocity;

		particle._force += force;
	}
}

void SimulatedScene::TimeIntegration(float deltaSecond)
{
	size_t particleCount = _particles.size();

	#pragma omp parallel for
	for (int i = 0; i < particleCount; ++i)
	{
		auto &particle = _particles[i];
		auto &nextParticle = _nextParticles[i];

		// Integrate velocity
		nextParticle._velocity = particle._velocity + deltaSecond * (particle._force / _particleMass);

		// Integrate position
		nextParticle._position = particle._position + deltaSecond * nextParticle._velocity;
	}
}

void SimulatedScene::ResolveCollision()
{

}

// Reflect the particle positions to the render system
void SimulatedScene::ApplyParticlePositions()
{
	size_t particleCount = _particles.size();
	for (size_t i = 0; i < particleCount; ++i)
	{
		glm::vec3 particlePosition = _particles[i]._position;
		for (size_t j = 0; j < VERTICES_IN_PARTICLE.size(); ++j)
		{
			_particleVertices[i * VERTICES_IN_PARTICLE.size() + j].pos = particlePosition;
		}
	}

	_particleModel->UpdateVertexBuffer(_particleVertices);
}

glm::vec3 SimulatedScene::GetWindVelocityAt(glm::vec3 samplePosition)
{
	return glm::vec3{}; // Temp
}
