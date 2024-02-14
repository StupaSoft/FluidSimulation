#include "SimulatedScene.h"

const glm::vec3 SimulatedScene::GRAVITY = glm::vec3(0.0f, -9.8f, 0.0f);

void SimulatedScene::InitializeParticles(float particleRadius, float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	_particleRadius = particleRadius;
	_kernel = Kernel(_particleRadius * simulationParameters._kernelRadiusFactor);

	// Setup
	size_t xCount = std::ceil((xRange.g - xRange.r) / particleDistance);
	size_t yCount = std::ceil((yRange.g - yRange.r) / particleDistance);
	size_t zCount = std::ceil((zRange.g - zRange.r) / particleDistance);

	_particleCount = xCount * yCount * zCount;
	glm::vec3 startingPoint = glm::vec3(xRange.r, yRange.r, zRange.r);

	// Prepare render data
	_particleVertices.clear();
	_particleVertices.resize(_particleCount * VERTICES_IN_PARTICLE.size());

	_particleIndices.clear();
	_particleIndices.resize(_particleCount * INDICES_IN_PARTICLE.size());

	// Prepare particles themselves
	_positions = std::vector<glm::vec3>(_particleCount);
	_velocities = std::vector<glm::vec3>(_particleCount);
	_forces = std::vector<glm::vec3>(_particleCount);
	_densities = std::vector<float>(_particleCount);
	_pressures = std::vector<float>(_particleCount);
	_pressureForces = std::vector<glm::vec3>(_particleCount);

	_nextPositions = std::vector<glm::vec3>(_particleCount);
	_nextVelocities = std::vector<glm::vec3>(_particleCount);

	// Initialize particles
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
				_positions[particleIndex] = startingPoint + glm::vec3(x, y, z) * particleDistance;
			}
		}
	}

	// Create models
	if (_particleModel == nullptr)
	{
		_particleModel = _vulkanCore->AddModel<MeshModel>();
		_particleModel->LoadMesh(_particleVertices, _particleIndices);
		_particleModel->LoadShaders("Shaders/ParticleVertex.spv", "Shaders/ParticleFragment.spv");

		MeshModel::Material particleMat
		{
			._color = glm::vec4(0.0f, 0.2f, 1.0f, 1.0f),
			._glossiness = 1.0f
		};
		_particleModel->SetMaterial(std::move(particleMat));
		_particleModel->AddMeshObject();
	}

	ApplyParticlePositions();

	// Build the hash grid and BVH
	_hashGrid = std::make_unique<HashGrid>(_particleCount, _gridResolution, 2.0f * _particleRadius * simulationParameters._kernelRadiusFactor);
}

void SimulatedScene::AddProp(const std::string &OBJPath, const std::string &texturePath, bool isVisible, bool isCollidable)
{
	auto obj = LoadOBJ(OBJPath);

	auto propModel = _vulkanCore->AddModel<MeshModel>();
	propModel->LoadMesh(std::get<0>(obj), std::get<1>(obj));
	propModel->LoadShaders("Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv");
	propModel->LoadTexture(texturePath);

	auto propObject = propModel->AddMeshObject();
	propObject->SetVisible(isVisible);
	propObject->SetCollidable(isCollidable);

	_bvh->AddPropObject(propObject);
}

void SimulatedScene::BeginTimeStep()
{
	_hashGrid->UpdateGrid(_positions);
	UpdateDensities();
}

void SimulatedScene::EndTimeStep()
{
	// Apply velocities and positions are applied
	std::copy(_nextPositions.cbegin(), _nextPositions.cend(), _positions.begin());
	std::copy(_nextVelocities.cbegin(), _nextVelocities.cend(), _velocities.begin());

	// Set other components to zero
	std::fill(_forces.begin(), _forces.end(), glm::vec3{});
	std::fill(_densities.begin(), _densities.end(), 0.0f);
	std::fill(_pressures.begin(), _pressures.end(), 0.0f);
	std::fill(_pressureForces.begin(), _pressureForces.end(), glm::vec3{});
}

void SimulatedScene::Update(float deltaSecond)
{
	if (!_play) return;

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
		// Apply gravity
		glm::vec3 externalForce = simulationParameters._particleMass * GRAVITY;

		// Apply wind forces
		glm::vec3 relativeVelocity = _velocities[particleIndex] - GetWindVelocityAt(_positions[particleIndex]);
		externalForce += -simulationParameters._dragCoefficient * relativeVelocity;

		_forces[particleIndex] += externalForce;
	}
}

void SimulatedScene::AccumulateViscosityForce(float deltaSecond)
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		_hashGrid->ForEachNeighborParticle
		(
			_positions,
			particleIndex,
			[&](size_t neighborIndex)
			{
				float distance = glm::distance(_positions[particleIndex], _positions[neighborIndex]);
				_forces[particleIndex] += simulationParameters._viscosityCoefficient * (simulationParameters._particleMass * simulationParameters._particleMass) * (_velocities[neighborIndex] - _velocities[particleIndex]) * _kernel.SecondDerivative(distance) / _densities[neighborIndex];
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
		float eosScale = simulationParameters._targetDensity * (simulationParameters._soundSpeed * simulationParameters._soundSpeed) / simulationParameters._eosExponent;
		_pressures[particleIndex] = ComputePressureFromEOS(_densities[particleIndex], simulationParameters._targetDensity, eosScale, simulationParameters._eosExponent);
	}

	// Compute pressure forces from the pressures
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		_hashGrid->ForEachNeighborParticle
		(
			_positions,
			particleIndex,
			[&](size_t neighborIndex)
			{
				float distance = glm::distance(_positions[particleIndex], _positions[neighborIndex]);
				if (distance > 0.0f)
				{
					glm::vec3 direction = (_positions[neighborIndex] - _positions[particleIndex]) / distance;
					_pressureForces[particleIndex] -=
						(simulationParameters._particleMass * simulationParameters._particleMass) * 
						_kernel.Gradient(distance, direction) * 
						(_pressures[particleIndex] / (_densities[particleIndex] * _densities[particleIndex]) + _pressures[neighborIndex] / (_densities[neighborIndex] * _densities[neighborIndex]));
				}
			}
		);

		_forces[particleIndex] += _pressureForces[particleIndex]; // Temp
	}
}

void SimulatedScene::ResolveCollision()
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		// Check if the new position is penetrating the surface
		Intersection intersection{};
		if (_bvh->GetIntersection(_positions[particleIndex], _nextPositions[particleIndex], &intersection))
		{
			// Target point is the closest non-penetrating position from the current position.
			glm::vec3 targetNormal = intersection.normal;
			glm::vec3 targetPoint = intersection.point + _particleRadius * targetNormal * 0.1f;
			glm::vec3 collisionPointVelocity = intersection.pointVelocity;

			// Get new candidate relative velocities from the target point
			glm::vec3 relativeVelocity = _nextVelocities[particleIndex] - collisionPointVelocity;
			float normalDotRelativeVelocity = glm::dot(targetNormal, relativeVelocity);
			glm::vec3 relativeVelocityN = normalDotRelativeVelocity * targetNormal;
			glm::vec3 relativeVelocityT = relativeVelocity - relativeVelocityN;

			// Check if the velocity is facing ooposite direction of the surface normal
			if (normalDotRelativeVelocity < 0.0f)
			{
				// Apply restitution coefficient to the surface normal component of the velocity
				glm::vec3 deltaRelativeVelocityN = (-simulationParameters._restitutionCoefficient - 1.0f) * relativeVelocityN;
				relativeVelocityN *= -simulationParameters._restitutionCoefficient;

				// Apply friction to the tangential component of the velocity
				if (relativeVelocityT.length() > 0.0f)
				{
					float frictionScale = std::max(1.0f - simulationParameters._frictionCoefficient * deltaRelativeVelocityN.length() / relativeVelocityT.length(), 0.0f);
					relativeVelocityT *= frictionScale;
				}

				// Apply the velocity
				_nextVelocities[particleIndex] = relativeVelocityN + relativeVelocityT + collisionPointVelocity;
			}

			// Apply the position
			_nextPositions[particleIndex] = targetPoint;
		}
	}
}

void SimulatedScene::TimeIntegration(float deltaSecond)
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		// Integrate velocity
		_nextVelocities[particleIndex] = _velocities[particleIndex] + deltaSecond * (_forces[particleIndex] / simulationParameters._particleMass);

		// Integrate position
		_nextPositions[particleIndex] = _positions[particleIndex] + deltaSecond * _nextVelocities[particleIndex];
	}
}

glm::vec3 SimulatedScene::GetWindVelocityAt(glm::vec3 samplePosition)
{
	return glm::vec3{}; // Temp
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
		float sum = _kernel.GetValue(0.0f);
		_hashGrid->ForEachNeighborParticle
		(
			_positions,
			particleIndex,
			[&](size_t neighborIndex)
			{
				float distance = glm::distance(_positions[particleIndex], _positions[neighborIndex]);
				sum += _kernel.GetValue(distance);
			}
		);

		_densities[particleIndex] = sum * simulationParameters._particleMass;
	}
}

// Reflect the particle positions to the render system
void SimulatedScene::ApplyParticlePositions()
{
#pragma omp parallel for
	for (size_t i = 0; i < _particleCount; ++i)
	{
		glm::vec3 particlePosition = _positions[i];
		for (size_t j = 0; j < VERTICES_IN_PARTICLE.size(); ++j)
		{
			_particleVertices[i * VERTICES_IN_PARTICLE.size() + j].pos = particlePosition;
		}
	}

	_particleModel->UpdateVertices(_particleVertices);
}
