import SimulationModule;

ConstantBuffer<GridSetup> gridSetup;
RWStructuredBuffer<uint> accumulations;

[vk::push_constant] ConstantBuffer<uint> prefixStep;

[shader("compute")]
[numthreads(1024, 1, 1)]
void mainUp(uint3 globalThreadID : SV_DispatchThreadID)
{
    // Prefix sum using Blelloch's algorithm
    // Up-sweep phase
    uint bucketCount = gridSetup.dimension.x * gridSetup.dimension.y * gridSetup.dimension.z;
    uint step = prefixStep; // 0 to log(2, n) - 1

    uint k = globalThreadID.x * (1 << (step + 1));

    uint bucketIndex1 = k + (1 << step) - 1;
    uint bucketIndex2 = k + (1 << (step + 1)) - 1;

    if (bucketIndex1 < bucketCount && bucketIndex2 < bucketCount)
    {
        accumulations[bucketIndex2] = accumulations[bucketIndex1] + accumulations[bucketIndex2];
    }
}

[shader("compute")]
[numthreads(1024, 1, 1)]
void mainTurn(uint3 globalThreadID : SV_DispatchThreadID)
{
    // Turn the phase between the up-sweep phase and down-sweep phase
    uint bucketCount = gridSetup.dimension.x * gridSetup.dimension.y * gridSetup.dimension.y;
    accumulations[bucketCount - 1] = 0;
}

[shader("compute")]
[numthreads(1024, 1, 1)]
void mainDown(uint3 globalThreadID : SV_DispatchThreadID)
{
    // Prefix sum using Blelloch's algorithm
    // Down-sweep phase
    uint bucketCount = gridSetup.dimension.x * gridSetup.dimension.y * gridSetup.dimension.y;
    uint step = prefixStep; // log(2, n) - 1 down to 0

    uint k = globalThreadID.x * (1 << (step + 1));

    uint bucketIndex1 = k + (1 << step) - 1;
    uint bucketIndex2 = k + (1 << (step + 1)) - 1;

    if (bucketIndex1 < bucketCount && bucketIndex2 < bucketCount)
    {
        uint temp = accumulations[bucketIndex1];
        accumulations[bucketIndex1] = accumulations[bucketIndex2];
        accumulations[bucketIndex2] = temp + accumulations[bucketIndex2];
    }
}

