import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;

RWStructuredBuffer<uint> hashResults;
RWStructuredBuffer<uint> accumulations; // After this stage, the accumulations will contain exclusive end indices of each bucket 
RWStructuredBuffer<uint> buckets; // [# of particles]

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	uint hashKey = hashResults[particleIndex];
    uint entry = 0;
	InterlockedAdd(accumulations[hashKey], 1, entry);
	buckets[entry] = particleIndex;
}
