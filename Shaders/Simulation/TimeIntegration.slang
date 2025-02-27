import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<float3> velocities;
RWStructuredBuffer<float3> forces;
RWStructuredBuffer<float3> nextVelocities;
RWStructuredBuffer<float3> nextPositions;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	// Integrate velocity
	nextVelocities[particleIndex] = velocities[particleIndex] + simulationParameters.timeStep * (forces[particleIndex] / simulationParameters.particleMass);

	// Integrate position
	nextPositions[particleIndex] = positions[particleIndex] + simulationParameters.timeStep * nextVelocities[particleIndex];
}
