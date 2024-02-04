#include "SimulatedScene.h"

const float SimulatedScene::DRAG_COEFF = 1e-4;
const glm::vec3 SimulatedScene::GRAVITY = glm::vec3(0.0f, -9.8f, 0.0f);
const float SimulatedScene::SOUND_SPEED = 1.0f;
const float SimulatedScene::EOS_EXPONENT = 1.0f;
const glm::uvec3 SimulatedScene::GRID_RESOLUTION = glm::uvec3(100, 100, 100);
const float SimulatedScene::VISCOSITY_COEFF = 1e-4;

void SimulatedScene::AddLevel(const std::string &OBJPath, const std::string &texturePath)
{
	auto obj = LoadOBJ(OBJPath);

	auto levelModel = _vulkanCore->AddModel<MeshModel>();
	levelModel->LoadAssets(std::get<0>(obj), std::get<1>(obj), "Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv", texturePath);
	levelModel->AddMeshObject();

	_levelModels.emplace_back(std::move(levelModel));
}

void SimulatedScene::InitializeParticles(float particleMass, float particleRadius, float particleDistance, float targetDensity, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	_targetDensity = targetDensity;
	_kernelRadius = particleRadius * 4.0f;
	_kernel = Kernel(_kernelRadius);

	// Setup
	size_t xCount = std::ceil((xRange.g - xRange.r) / particleDistance);
	size_t yCount = std::ceil((yRange.g - yRange.r) / particleDistance);
	size_t zCount = std::ceil((zRange.g - zRange.r) / particleDistance);

	_particleCount = xCount * yCount * zCount;
	glm::vec3 startingPoint = glm::vec3(xRange.r, yRange.r, zRange.r);

	// Prepare data structures
	_particles.resize(_particleCount);

	_particleVertices.clear();
	_particleVertices.resize(_particleCount * VERTICES_IN_PARTICLE.size());

	_particleIndices.clear();
	_particleIndices.resize(_particleCount * INDICES_IN_PARTICLE.size());

	// Initialize particles
	_particleMass = particleMass;
	for (size_t i = 0; i < _particleCount; ++i)
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

	for (size_t i = 0; i < _particleCount; ++i)
	{
		auto &particle = _particles[i];
		particle._force = glm::vec3(rand() % 20, 40 + rand() % 10, rand() % 20); // Temp
	}

	// Make a copy of the particles that will be used in simulation.
	_nextParticles = _particles;
	for (size_t i = 0; i < _particleCount; ++i)
	{
		_nextParticles[i]._force = glm::vec3{};
	}

	// Create models
	_particleModel = _vulkanCore->AddModel<MeshModel>();
	_particleModel->LoadAssets(_particleVertices, _particleIndices, "Shaders/ParticleVertex.spv", "Shaders/ParticleFragment.spv");

	MeshModel::Material particleMat
	{
		._color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
	};
	_particleModel->SetMaterial(std::move(particleMat));
	_particleModel->AddMeshObject();

	ApplyParticlePositions();

	// Build the hash grid
	_hashGrid = std::make_unique<HashGrid>(_particles, GRID_RESOLUTION, 2.0f * _kernelRadius);
}

void SimulatedScene::BeginTimeStep()
{
	_hashGrid->UpdateGrid();
	UpdateDensities();
}

void SimulatedScene::EndTimeStep()
{
	// 1. Velocities and positions are applied
	// 2. Forces are set to zero
	_particles = _nextParticles;

	// Damplens noticeable noises
	ComputePseudoViscosity();
}

void SimulatedScene::Update(float deltaSecond)
{
	// Conduct simulation
	BeginTimeStep();

	AccumulateForces(deltaSecond);
	TimeIntegration(deltaSecond);
	ResolveCollision();

	EndTimeStep();

	// Reflect the particle status to the render system
	ApplyParticlePositions();
}

void SimulatedScene::AccumulateForces(float deltaSecond)
{
	AccumulateExternalForce(deltaSecond);
	AccumulateViscosityForce(deltaSecond);
	AccumulatePressureForce(deltaSecond);
}

void SimulatedScene::AccumulateExternalForce(float deltaSecond)
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		auto &particle = _particles[particleIndex];

		// Apply gravity
		glm::vec3 externalForce = _particleMass * GRAVITY;

		// Apply wind forces
		glm::vec3 relativeVelocity = particle._velocity - GetWindVelocityAt(particle._position);
		externalForce += -DRAG_COEFF * relativeVelocity;

		particle._force += externalForce;
	}
}

void SimulatedScene::AccumulateViscosityForce(float deltaSecond)
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		auto &particle = _particles[particleIndex];

		_hashGrid->ForEachNeighborParticle
		(
			particleIndex,
			[&](size_t neighborIndex)
			{
				const auto &neighborParticle = _particles[neighborIndex];

				float distance = glm::distance(particle._position, neighborParticle._position);
				particle._force += VISCOSITY_COEFF * (_particleMass * _particleMass) * (neighborParticle._velocity - particle._velocity) * _kernel.SecondDerivative(distance) / neighborParticle._density;
			}
		);
	}
}

void SimulatedScene::AccumulatePressureForce(float deltaSecond)
{
	// Compute pressures
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		auto &particle = _particles[particleIndex];

		float eosScale = _targetDensity * (SOUND_SPEED * SOUND_SPEED) / EOS_EXPONENT;
		particle._pressure = ComputePressureFromEOS(particle._density, _targetDensity, eosScale, EOS_EXPONENT);
	}

	// Compute pressure forces from the pressures
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		auto &particle = _particles[particleIndex];

		_hashGrid->ForEachNeighborParticle
		(
			particleIndex,
			[&](size_t neighborIndex)
			{
				const auto &neighborParticle = _particles[neighborIndex];

				float distance = glm::distance(particle._position, neighborParticle._position);
				if (distance > 0.0f)
				{
					glm::vec3 direction = (neighborParticle._position - particle._position) / distance;
					particle._pressureForce -=
						(_particleMass * _particleMass) * 
						_kernel.Gradient(distance, direction) * 
						(particle._pressure / (particle._density * particle._density) + neighborParticle._pressure / (neighborParticle._density * neighborParticle._density));
				}
			}
		);

		particle._force += particle._pressureForce; // Temp
	}
}

void SimulatedScene::TimeIntegration(float deltaSecond)
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		auto &particle = _particles[particleIndex];
		auto &nextParticle = _nextParticles[particleIndex];

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
	#pragma omp parallel for
	for (size_t i = 0; i < _particleCount; ++i)
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

void SimulatedScene::ComputePseudoViscosity()
{

}

float SimulatedScene::ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent)
{
	float pressure = eosScale * (std::pow(density / targetDensity, eosExponent) - 1.0f) / eosExponent;
	if (pressure < 0.0f) pressure = 0.0f;
	return pressure;
}

void SimulatedScene::UpdateDensities()
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		auto &particle = _particles[particleIndex];

		float sum = 0.0f;
		_hashGrid->ForEachNeighborParticle
		(
			particleIndex,
			[&](size_t neighborIndex)
			{
				const auto &neighborParticle = _particles[neighborIndex];

				float distance = glm::distance(_particles[particleIndex]._position, neighborParticle._position);
				sum += _kernel.GetValue(distance);
			}
		);

		particle._density = sum * _particleMass;
	}
}

glm::vec3 SimulatedScene::Interpolate(size_t particleIndex, const std::vector<glm::vec3> &values) const
{
	assert(values.size() == _particles.size());

	const auto &particle = _particles[particleIndex];

	glm::vec3 sum{};

	_hashGrid->ForEachNeighborParticle
	(
		particleIndex,
		[&](size_t neighborIndex)
		{
			const auto &neighborParticle = _particles[neighborIndex];

			float distance = glm::distance(particle._position, neighborParticle._position);
			float weight = _particleMass / neighborParticle._density * _kernel.GetValue(distance);
			sum += weight * values[neighborIndex];
		});

	return sum;
}

glm::vec3 SimulatedScene::GradientAt(size_t particleIndex, const std::vector<glm::vec3> &values) const
{
	assert(values.size() == _particles.size());

	const auto &particle = _particles[particleIndex];

	glm::vec3 sum{};

	_hashGrid->ForEachNeighborParticle
	(
		particleIndex,
		[&](size_t neighborIndex)
		{
			const auto &neighborParticle = _particles[neighborIndex];
			float distance = glm::distance(particle._position, neighborParticle._position);
			if (distance > 0.0f)
			{
				auto direction = (neighborParticle._position - particle._position) / distance;
				sum += particle._density * 
					_particleMass * 
					(values[particleIndex] / (particle._density * particle._density) + values[neighborIndex] / (neighborParticle._density * neighborParticle._density)) *
					_kernel.Gradient(distance, direction);
			}
		});

	return sum;
}

float SimulatedScene::LaplacianAt(size_t particleIndex, const std::vector<float> &values) const
{
	assert(values.size() == _particles.size());

	const auto &particle = _particles[particleIndex];

	// Temp - use neighbor list instead...?
	float sum = 0.0f;

	_hashGrid->ForEachNeighborParticle
	(
		particleIndex,
		[&](size_t neighborIndex)
		{
			const auto &neighborParticle = _particles[neighborIndex];
			float distance = glm::distance(particle._position, neighborParticle._position);
			if (distance > 0.0f)
			{
				sum += _particleMass * _kernel.SecondDerivative(distance) * (values[neighborIndex] - values[particleIndex]) / neighborParticle._density;
			}
		});

	return sum;
}
