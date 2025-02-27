import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<GridSetup> gridSetup;

RWStructuredBuffer<float3> nextVelocities;
RWStructuredBuffer<float3> nextPositions;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<float3> velocities;
RWStructuredBuffer<float3> forces;
RWStructuredBuffer<float> densities;
RWStructuredBuffer<float> pressures;
RWStructuredBuffer<uint> accumulations;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
    // Reset per-particle values
	uint particleIndex = globalThreadID.x;
	if (particleIndex < simulationSetup.particleCount)
    {
        positions[particleIndex] = nextPositions[particleIndex];
        velocities[particleIndex] = nextVelocities[particleIndex];

        forces[particleIndex] = 0.0f.xxx;
        densities[particleIndex] = 0.0f;
        pressures[particleIndex] = 0.0f;
    }

    // Reset per-bucket values
    uint bucketIndex = globalThreadID.x;
    uint bucketCount = gridSetup.dimension.x * gridSetup.dimension.y * gridSetup.dimension.z;
    if (bucketIndex < bucketCount)
    {
        accumulations[bucketIndex] = 0;
    }
}