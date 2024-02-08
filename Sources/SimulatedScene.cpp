#include "SimulatedScene.h"

const float SimulatedScene::DRAG_COEFF = 1e-3;
const glm::vec3 SimulatedScene::GRAVITY = glm::vec3(0.0f, -9.8f, 0.0f);
const float SimulatedScene::SOUND_SPEED = 1.0f;
const float SimulatedScene::EOS_EXPONENT = 2.0f;
const glm::uvec3 SimulatedScene::GRID_RESOLUTION = glm::uvec3(100, 100, 100);
const float SimulatedScene::VISCOSITY_COEFF = 1e-3f;

void SimulatedScene::InitializeParticles(float particleRadius, float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	_particleRadius = particleRadius;

	_kernelRadius = particleRadius * 4.0f;
	_kernel = Kernel(_kernelRadius);

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
	_positions.resize(_particleCount);
	_velocities.resize(_particleCount);
	_forces.resize(_particleCount);
	_densities.resize(_particleCount);
	_pressures.resize(_particleCount);
	_pressureForces.resize(_particleCount);

	_nextPositions.resize(_particleCount);
	_nextVelocities.resize(_particleCount);

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
	_particleModel = _vulkanCore->AddModel<MeshModel>();
	_particleModel->LoadAssets(_particleVertices, _particleIndices, "Shaders/ParticleVertex.spv", "Shaders/ParticleFragment.spv");

	MeshModel::Material particleMat
	{
		._color = glm::vec4(0.0f, 0.2f, 1.0f, 1.0f),
		._glossiness = 1.0f
	};
	_particleModel->SetMaterial(std::move(particleMat));
	_particleModel->AddMeshObject();

	ApplyParticlePositions();

	// Build the hash grid and BVH
	_hashGrid = std::make_unique<HashGrid>(_particleCount, GRID_RESOLUTION, 2.0f * _kernelRadius);
}

void SimulatedScene::AddProp(const std::string &OBJPath, const std::string &texturePath, bool isVisible, bool isCollidable)
{
	auto obj = LoadOBJ(OBJPath);

	auto propModel = _vulkanCore->AddModel<MeshModel>();
	propModel->LoadAssets(std::get<0>(obj), std::get<1>(obj), "Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv", texturePath);
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
		glm::vec3 externalForce = _particleMass * GRAVITY;

		// Apply wind forces
		glm::vec3 relativeVelocity = _velocities[particleIndex] - GetWindVelocityAt(_positions[particleIndex]);
		externalForce += -DRAG_COEFF * relativeVelocity;

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
				_forces[particleIndex] += VISCOSITY_COEFF * (_particleMass * _particleMass) * (_velocities[neighborIndex] - _velocities[particleIndex]) * _kernel.SecondDerivative(distance) / _densities[neighborIndex];
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
		float eosScale = _targetDensity * (SOUND_SPEED * SOUND_SPEED) / EOS_EXPONENT;
		_pressures[particleIndex] = ComputePressureFromEOS(_densities[particleIndex], _targetDensity, eosScale, EOS_EXPONENT);
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
						(_particleMass * _particleMass) * 
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
			glm::vec3 targetPoint = intersection.point + _particleRadius * targetNormal * 0.5f;
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
				glm::vec3 deltaRelativeVelocityN = (-_restitutionCoefficient - 1.0f) * relativeVelocityN;
				relativeVelocityN *= -_restitutionCoefficient;

				// Apply friction to the tangential component of the velocity
				if (relativeVelocityT.length() > 0.0f)
				{
					float frictionScale = std::max(1.0f - _frictionCoefficient * deltaRelativeVelocityN.length() / relativeVelocityT.length(), 0.0f);
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
		_nextVelocities[particleIndex] = _velocities[particleIndex] + deltaSecond * (_forces[particleIndex] / _particleMass);

		// Integrate position
		_nextPositions[particleIndex] = _positions[particleIndex] + deltaSecond * _nextVelocities[particleIndex];
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
		float sum = 0.0f;
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

		_densities[particleIndex] = sum * _particleMass;
	}
}
