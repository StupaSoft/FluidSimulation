module MarchingCubesModule;

public static const float PI = 3.141592f;
public static const uint FRAC = 10000;
public static const uint CORNERS_IN_CELL = 8;
public static const uint CODES_COUNT = 256;
public static const uint INDICES_IN_CELL = 15;
public static const uint VERTICES_IN_CELL = 12;
public static const uint NORMAL_GROUP_COUNT= 4;

public struct ParticleProperty
{
    public uint particleCount;
    public float r1;
    public float r2;
    public float r3;
}

public struct MarchingCubesSetup
{
    public float2 xRange;
    public float2 yRange;
    public float2 zRange;
    public float voxelInterval;
    public float isovalue;

    public uint4 voxelDimension;
    public uint voxelCount;

    public uint4 cellDimension;
    public uint cellCount;

    public uint vertexCount;
}

public struct DrawArguments
{
    public uint indexCount;
    public uint instanceCount;
    public uint firstIndex;
    public int vertexOffset;
    public uint firstInstance;
}

public uint GetLinearIndex(uint3 volumeIndex, uint3 dimension)
{
    return volumeIndex.z * (dimension.x * dimension.y) + volumeIndex.y * (dimension.x) + volumeIndex.x;
}

public uint3 GetVolumeIndex(uint index, uint3 dimension)
{
    uint x = index % dimension.x;
    index = (index - x) / dimension.x;
    uint y = index % dimension.y;
    uint z = (index - y) / dimension.y;

    return uint3(x, y, z); 
}