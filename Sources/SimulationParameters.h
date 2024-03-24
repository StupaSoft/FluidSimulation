#pragma once

struct SimulationParameters
{
	alignas(4) float _particleRadius = 0.03f;
	alignas(4) float _particleMass = 0.05f;

	alignas(4) float _timeStep = 0.002f;

	alignas(16) glm::vec4 _gravitiy = glm::vec4(0.0f, -9.8f, 0.0f, 0.0f);

	alignas(4) float _targetDensity = 400.0f;
	alignas(4) float _soundSpeed = 2.5f;
	alignas(4) float _eosExponent = 0.2f;
	alignas(4) float _kernelRadiusFactor = 4.0f;

	alignas(4) float _dragCoefficient = 0.001f;
	alignas(4) float _viscosityCoefficient = 0.005f;
	alignas(4) float _restitutionCoefficient = 0.5f;
	alignas(4) float _frictionCoefficient = 0.1f;
};


