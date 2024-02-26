#pragma once

struct SimulationParameters
{
	float _particleRadius = 0.03f;
	float _particleMass = 0.05f;

	float _targetDensity = 400.0f;
	float _soundSpeed = 2.5f;
	float _eosExponent = 0.2f;
	float _kernelRadiusFactor = 4.0f;

	float _dragCoefficient = 0.001f;
	float _viscosityCoefficient = 0.005f;
	float _restitutionCoefficient = 0.5f;
	float _frictionCoefficient = 0.1f;
};


