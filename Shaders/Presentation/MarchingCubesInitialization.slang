import MarchingCubesModule;

ConstantBuffer<MarchingCubesSetup> setup;

RWStructuredBuffer<uint> voxelDensities;
RWStructuredBuffer<DrawArguments> drawArguments;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    // This invocation deals with a single voxel.
    uint voxelIndex = globalThreadID.x;
    if (voxelIndex < setup.voxelCount)
    {
        voxelDensities[voxelIndex] = 0;
    }

    // indexCount should be set to zero
    drawArguments[0].indexCount = 0;
}
