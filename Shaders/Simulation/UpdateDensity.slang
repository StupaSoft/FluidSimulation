import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<GridSetup> gridSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<uint> hashResults;
RWStructuredBuffer<uint> accumulations;
RWStructuredBuffer<uint> buckets; // [# of particles]
RWStructuredBuffer<uint> adjacentBuckets; // [# of particles * 8]
RWStructuredBuffer<float> densities;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	float kernelRange1 = simulationParameters.particleRadius * simulationParameters.kernelRadiusFactor;
	float kernelRange2 = kernelRange1 * kernelRange1;
	float kernelRange3 = kernelRange2 * kernelRange1;
	float3 particlePosition = positions[particleIndex];

	float sum = 0.0f;
	for (uint i = 0; i < OVERLAPPING_BUCKETS; ++i)
	{
		uint hashKey = adjacentBuckets[particleIndex * OVERLAPPING_BUCKETS + i];

		uint neighborStart = hashKey == 0 ? 0 : accumulations[hashKey - 1];
		uint neighborEnd = accumulations[hashKey]; // Exclusive end

		for (uint j = neighborStart; j < neighborEnd; ++j)
		{
			// Take itself into account
			uint neighborIndex = buckets[j];
			float distanceToNeighbor = distance(particlePosition, positions[neighborIndex]);
			if (distanceToNeighbor < kernelRange1)
			{
				sum += Kernel(distanceToNeighbor, kernelRange1, kernelRange2, kernelRange3);
			}
		}
	}

	densities[particleIndex] = sum * simulationParameters.particleMass;	
}