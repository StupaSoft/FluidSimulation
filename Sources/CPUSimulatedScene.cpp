#include "CPUSimulatedScene.h"

void CPUSimulatedScene::Register()
{
	VulkanCore::Get()->OnExecuteHost().AddListener
	(
		weak_from_this(),
		[this](float deltaSecond, uint32_t currentFrame)
		{
			Update();
		}
	);

	_onUpdateSimulationParameters.AddListener
	(
		weak_from_this(),
		[this](const SimulationParameters &simulationParameters)
		{
			_kernel = std::make_unique<Kernel>(simulationParameters._particleRadius * simulationParameters._kernelRadiusFactor);
		}
	);
}

void CPUSimulatedScene::InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	// Setup
	size_t xCount = std::lround(std::ceil((xRange.g - xRange.r) / particleDistance));
	size_t yCount = std::lround(std::ceil((yRange.g - yRange.r) / particleDistance));
	size_t zCount = std::lround(std::ceil((zRange.g - zRange.r) / particleDistance));

	_particleCount = xCount * yCount * zCount;
	glm::vec3 startingPoint = glm::vec3(xRange.r, yRange.r, zRange.r);

	// Prepare particles themselves
	_positions = std::vector<glm::vec3>(_particleCount);
	_velocities = std::vector<glm::vec3>(_particleCount);
	_forces = std::vector<glm::vec3>(_particleCount);
	_densities = std::vector<float>(_particleCount);
	_pressures = std::vector<float>(_particleCount);

	_nextPositions = std::vector<glm::vec3>(_particleCount);
	_nextVelocities = std::vector<glm::vec3>(_particleCount);

	// Place particles
	#pragma omp parallel for
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

	// Initialize hashed buckets
	_hashGrid = std::make_unique<HashGrid>(_particleCount, _gridDimension);
	_hashGrid->UpdateSpacing(2.0f * _simulationParameters->_particleRadius * _simulationParameters->_kernelRadiusFactor);
	_onUpdateSimulationParameters.AddListener
	(
		weak_from_this(),
		[this](const SimulationParameters &simulationParameters)
		{
			_hashGrid->UpdateSpacing(2.0f * simulationParameters._particleRadius * simulationParameters._kernelRadiusFactor);
		},
		PRIORITY_LOWEST,
		__FUNCTION__, 
		__LINE__
	);

	// Initialize renderers (marching cubes and billboards)
	_particlePositionInputBuffers = CreateBuffers(sizeof(glm::vec3) * _particleCount, VulkanCore::Get()->GetMaxFramesInFlight(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	InitializeRenderers(_particlePositionInputBuffers, _particleCount);
	
	// Launch
	_isPlaying = true;
	ApplyRenderMode(_particleRenderingMode);
}

void CPUSimulatedScene::BeginTimeStep()
{
	_hashGrid->UpdateGrid(_positions);
	UpdateDensities();
}

void CPUSimulatedScene::EndTimeStep()
{
	// Apply velocities and positions are applied
	std::copy(_nextPositions.cbegin(), _nextPositions.cend(), _positions.begin());
	std::copy(_nextVelocities.cbegin(), _nextVelocities.cend(), _velocities.begin());

	// Set other components to zero
	std::fill(_forces.begin(), _forces.end(), glm::vec3{});
	std::fill(_densities.begin(), _densities.end(), 0.0f);
	std::fill(_pressures.begin(), _pressures.end(), 0.0f);
}

void CPUSimulatedScene::Update()
{
	if (!_isPlaying) return;

	// Conduct simulation
	BeginTimeStep();

	AccumulateForces();
	TimeIntegration(_simulationParameters->_timeStep);
	ResolveCollision();

	EndTimeStep();

	// Reflect the particle status to the render system
	Applypositions();
}

void CPUSimulatedScene::AccumulateForces()
{
	AccumulateExternalForce();
	AccumulateViscosityForce();
	AccumulatePressureForce();
}

void CPUSimulatedScene::AccumulateExternalForce()
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		// Apply gravity
		glm::vec3 externalForce = _simulationParameters->_particleMass * _simulationParameters->_gravitiy.xyz;

		// Apply wind forces
		glm::vec3 relativeVelocity = _velocities[particleIndex] - GetWindVelocityAt(_positions[particleIndex]);
		externalForce += -_simulationParameters->_dragCoefficient * relativeVelocity;

		_forces[particleIndex] += externalForce;
	}
}

void CPUSimulatedScene::AccumulateViscosityForce()
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
				_forces[particleIndex] += _simulationParameters->_viscosityCoefficient * (_simulationParameters->_particleMass * _simulationParameters->_particleMass) * (_velocities[neighborIndex] - _velocities[particleIndex]) * _kernel->SecondDerivative(distance) / _densities[neighborIndex];
			}
		);
	}
}

void CPUSimulatedScene::AccumulatePressureForce()
{
	// Compute pressures
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		float eosScale = _simulationParameters->_targetDensity * (_simulationParameters->_soundSpeed * _simulationParameters->_soundSpeed) / _simulationParameters->_eosExponent;
		_pressures[particleIndex] = ComputePressureFromEOS(_densities[particleIndex], _simulationParameters->_targetDensity, eosScale, _simulationParameters->_eosExponent);
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
					_forces[particleIndex] -=
						(_simulationParameters->_particleMass * _simulationParameters->_particleMass) * 
						_kernel->Gradient(distance, direction) * 
						(_pressures[particleIndex] / (_densities[particleIndex] * _densities[particleIndex]) + _pressures[neighborIndex] / (_densities[neighborIndex] * _densities[neighborIndex]));
				}
			}
		);
	}
}

void CPUSimulatedScene::ResolveCollision()
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		// Check if the new position is penetrating any surface
		Intersection intersection{};
		if (_bvh->GetIntersection(_positions[particleIndex], _nextPositions[particleIndex], &intersection))
		{
			// Target point is the closest non-penetrating position from the current position.
			glm::vec3 targetNormal = intersection._normal;
			glm::vec3 targetPoint = intersection._point + _simulationParameters->_particleRadius * targetNormal * 0.5f;
			glm::vec3 collisionPointVelocity = intersection._pointVelocity;

			// Get new candidate relative velocities from the target point
			glm::vec3 relativeVelocity = _nextVelocities[particleIndex] - collisionPointVelocity;
			float normalDotRelativeVelocity = glm::dot(targetNormal, relativeVelocity);
			glm::vec3 relativeVelocityN = normalDotRelativeVelocity * targetNormal;
			glm::vec3 relativeVelocityT = relativeVelocity - relativeVelocityN;

			// Check if the velocity is facing ooposite direction of the surface normal
			if (normalDotRelativeVelocity < 0.0f)
			{
				// Apply restitution coefficient to the surface normal component of the velocity
				glm::vec3 deltaRelativeVelocityN = (-_simulationParameters->_restitutionCoefficient - 1.0f) * relativeVelocityN;
				relativeVelocityN *= -_simulationParameters->_restitutionCoefficient;

				// Apply friction to the tangential component of the velocity
				if (relativeVelocityT.length() > 0.0f)
				{
					float frictionScale = std::max(1.0f - _simulationParameters->_frictionCoefficient * deltaRelativeVelocityN.length() / relativeVelocityT.length(), 0.0f);
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

void CPUSimulatedScene::TimeIntegration(float deltaSecond)
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		// Integrate velocity
		_nextVelocities[particleIndex] = _velocities[particleIndex] + deltaSecond * (_forces[particleIndex] / _simulationParameters->_particleMass);

		// Integrate position
		_nextPositions[particleIndex] = _positions[particleIndex] + deltaSecond * _nextVelocities[particleIndex];
	}
}

glm::vec3 CPUSimulatedScene::GetWindVelocityAt(glm::vec3 samplePosition)
{
	return glm::vec3{}; // Temp
}

float CPUSimulatedScene::ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent)
{
	float pressure = eosScale * (std::pow(density / targetDensity, eosExponent) - 1.0f) / eosExponent;
	if (pressure < 0.0f) pressure = 0.0f;
	return pressure;
}

void CPUSimulatedScene::UpdateDensities()
{
	#pragma omp parallel for
	for (size_t particleIndex = 0; particleIndex < _particleCount; ++particleIndex)
	{
		float sum = _kernel->GetValue(0.0f);
		_hashGrid->ForEachNeighborParticle
		(
			_positions,
			particleIndex,
			[&](size_t neighborIndex)
			{
				float distance = glm::distance(_positions[particleIndex], _positions[neighborIndex]);
				sum += _kernel->GetValue(distance);
			}
		);

		_densities[particleIndex] = sum * _simulationParameters->_particleMass;
	}
}

// Reflect the particle positions to the render system
void CPUSimulatedScene::Applypositions()
{
	_particlePositionInputBuffers[VulkanCore::Get()->GetCurrentFrame()]->CopyFrom(_positions.data());
}