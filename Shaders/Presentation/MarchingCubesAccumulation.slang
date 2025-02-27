import MarchingCubesModule;
import SimulationModule;

ConstantBuffer<ParticleProperty> particleProperty;
ConstantBuffer<MarchingCubesSetup> setup;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<uint> voxelDensities;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    // An invocation deals with a single particle
    uint particleIndex = globalThreadID.x;
    if (particleIndex >= particleProperty.particleCount) return;

    float3 particlePosition = positions[particleIndex];
    if (particlePosition.x < setup.xRange.x || particlePosition.x > setup.xRange.y) return;
    if (particlePosition.y < setup.yRange.x || particlePosition.y > setup.yRange.y) return;
    if (particlePosition.z < setup.zRange.x || particlePosition.z > setup.zRange.y) return;

    // Get voxel ranges
    float3 relativePosition = particlePosition - float3(setup.xRange.x, setup.yRange.x, setup.zRange.x);

    uint voxelIndexMinX = max(int((relativePosition.x - particleProperty.r1) / setup.voxelInterval), 0);
    uint voxelIndexMaxX = min(int((relativePosition.x + particleProperty.r1) / setup.voxelInterval), setup.voxelDimension.x - 1);

    uint voxelIndexMinY = max(int((relativePosition.y - particleProperty.r1) / setup.voxelInterval), 0);
    uint voxelIndexMaxY = min(int((relativePosition.y + particleProperty.r1) / setup.voxelInterval), setup.voxelDimension.y - 1);

    uint voxelIndexMinZ = max(int((relativePosition.z - particleProperty.r1) / setup.voxelInterval), 0);
    uint voxelIndexMaxZ = min(int((relativePosition.z + particleProperty.r1) / setup.voxelInterval), setup.voxelDimension.z - 1);

    // Iterate through adjacent voxels and accumulate kernel values
    float3 voxelOrigin = float3(setup.xRange.x, setup.yRange.x, setup.zRange.x);
    for (uint z = voxelIndexMinZ; z <= voxelIndexMaxZ; ++z)
    {
        for (uint y = voxelIndexMinY; y <= voxelIndexMaxY; ++y)
        {
            for (uint x = voxelIndexMinX; x <= voxelIndexMaxX; ++x)
            {
                uint3 voxelVolumeIndex = uint3(x, y, z);
                float3 voxelPosition = voxelOrigin + voxelVolumeIndex * setup.voxelInterval;
                float dist = distance(particlePosition, voxelPosition);

                uint kernelValue = uint(Kernel(dist, particleProperty.r1, particleProperty.r2, particleProperty.r3) * FRAC);
                uint voxelLinearIndex = GetLinearIndex(voxelVolumeIndex, setup.voxelDimension.xyz);
                InterlockedAdd(voxelDensities[voxelLinearIndex], kernelValue);
            }
        }
    }
}
