import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<float> densities;
RWStructuredBuffer<float> pressures;

float ComputePressureFromEOS(float density, float targetDensity, float eosScale, float eosExponent)
{
    float pressure = (eosScale * (pow(density / targetDensity, eosExponent) - 1.0f)) / eosExponent;
    if (pressure < 0.0f)
    {
        pressure = 0.0f;
    }
    return pressure;
}

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	float eosScale = simulationParameters.targetDensity * (simulationParameters.soundSpeed * simulationParameters.soundSpeed) / simulationParameters.eosExponent;
	pressures[particleIndex] = ComputePressureFromEOS(densities[particleIndex], simulationParameters.targetDensity, eosScale, simulationParameters.eosExponent);
}
