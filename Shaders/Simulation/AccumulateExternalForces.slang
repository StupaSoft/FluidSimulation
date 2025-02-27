import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<float3> velocities;
RWStructuredBuffer<float3> forces;

float3 GetWindVelocityAt(float3 pos)
{
    return 0.0f.xxx;
}

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	// Apply gravity
	float3 externalForce = simulationParameters.particleMass * simulationParameters.gravitiy.xyz;

	// Apply wind forces
	float3 relativeVelocity = velocities[particleIndex] - GetWindVelocityAt(positions[particleIndex]);
	externalForce += -simulationParameters.dragCoefficient * relativeVelocity;

	forces[particleIndex] += externalForce;
}
