import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<GridSetup> gridSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<uint> hashResults;
RWStructuredBuffer<uint> accumulations;
RWStructuredBuffer<uint> buckets; // [# of particles]
RWStructuredBuffer<uint> adjacentBuckets; // [# of particles * 8]
RWStructuredBuffer<float3> velocities;
RWStructuredBuffer<float> densities;
RWStructuredBuffer<float> pressures;
RWStructuredBuffer<float3> forces;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
	uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	float3 particlePosition = positions[particleIndex];
	float3 particleVelocity = velocities[particleIndex];
	float pressureOverSquaredDensity = pressures[particleIndex] / (densities[particleIndex] * densities[particleIndex]);
	float kernelRange1 = simulationParameters.particleRadius * simulationParameters.kernelRadiusFactor;
	float kernelRange4 = pow(kernelRange1, 4.0f);
	float kernelRange5 = kernelRange4 * kernelRange1;
	float squaredMass = simulationParameters.particleMass * simulationParameters.particleMass;

	float3 sum = 0.0f.xxx;
	for (uint i = 0; i < OVERLAPPING_BUCKETS; ++i)
	{
		uint hashKey = adjacentBuckets[particleIndex * OVERLAPPING_BUCKETS + i];

		uint neighborStart = hashKey == 0 ? 0 : accumulations[hashKey - 1];
		uint neighborEnd = accumulations[hashKey]; // Exclusive end

		for (uint j = neighborStart; j < neighborEnd; ++j)
		{
			uint neighborIndex = buckets[j];
			if (particleIndex != neighborIndex)
			{
				float distanceToNeighbor = distance(particlePosition, positions[neighborIndex]) + EPSILON;
				if (distanceToNeighbor < kernelRange1)
				{
					float3 direction = (positions[neighborIndex] - particlePosition) / distanceToNeighbor;

					// Accumulate pressure forces
					sum -=
						squaredMass * 
						Gradient(distanceToNeighbor, direction, kernelRange1, kernelRange4) * 
						(pressureOverSquaredDensity + pressures[neighborIndex] / (densities[neighborIndex] * densities[neighborIndex]));

					// Accumulate viscosity forces
					sum +=
						simulationParameters.viscosityCoefficient * 
						squaredMass * 
						(velocities[neighborIndex] - particleVelocity) * 
						SecondDerivative(distanceToNeighbor, kernelRange1, kernelRange5) / densities[neighborIndex];
				}
			}
		}
	}

	forces[particleIndex] += sum;
}