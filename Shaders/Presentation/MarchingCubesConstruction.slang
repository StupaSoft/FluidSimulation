import MarchingCubesModule;
import SimulationModule;
import RenderModule;

ConstantBuffer<MarchingCubesSetup> setup;

RWStructuredBuffer<uint> voxelDensities;
RWStructuredBuffer<uint> indexTable;
RWStructuredBuffer<Vertex> vertices;
RWStructuredBuffer<uint> indices;
RWStructuredBuffer<DrawArguments> drawArguments;

// Vertex index -> Corner indices of the edge on which the vertex lies
static const uint2 VERTEX_TO_CORNERS[VERTICES_IN_CELL] = uint2[]
(
    uint2(0, 1), // Vertex 0 is between the corner 0 and corner 1
    uint2(1, 3), // Vertex 1
    uint2(2, 3), // Vertex 2
    uint2(0, 2), // Vertex 3
    uint2(4, 5), // Vertex 4
    uint2(5, 7), // Vertex 5
    uint2(6, 7), // Vertex 6
    uint2(4, 6), // Vertex 7
    uint2(0, 4), // Vertex 8
    uint2(1, 5), // Vertex 9
    uint2(3, 7), // Vertex 10
    uint2(2, 6) // Vertex 11
);

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    // This invocation deals with a single cell
    uint cellLinearIndex = globalThreadID.x;
    if (cellLinearIndex >= setup.cellCount) return;

    uint3 cellVolumeIndex = GetVolumeIndex(cellLinearIndex, setup.cellDimension.xyz); // The volume index of this cell
    float3 voxelOrigin = float3(setup.xRange.x, setup.yRange.x, setup.zRange.x);

    // Classify sparse and dense corner voxels
    uint code = 0;
    float cornerDensities[CORNERS_IN_CELL]; // Number of particles that belong to each corner voxel
    float3 voxelPositions[CORNERS_IN_CELL]; // Positions of voxels
    float3 voxelNormals[CORNERS_IN_CELL];
    for (uint i = 0; i < CORNERS_IN_CELL; ++i)
    {
        uint cornerX = (i >> 0) & 0x1;
        uint cornerY = (i >> 1) & 0x1;
        uint cornerZ = (i >> 2) & 0x1;
        uint3 voxelVolumeIndex = cellVolumeIndex + uint3(cornerX, cornerY, cornerZ);

        uint voxelLinearIndex = GetLinearIndex(voxelVolumeIndex, setup.voxelDimension.xyz);
        cornerDensities[i] = float(voxelDensities[voxelLinearIndex]) / FRAC;
        voxelPositions[i] = voxelOrigin + voxelVolumeIndex * setup.voxelInterval;

        // Find normals only for voxels between which lie vertex 0, 3, 8
        if (i == 0 || i == 1 || i == 2 || i == 4)
        {
            float3 normal;
            normal.x = float(voxelDensities[GetLinearIndex(voxelVolumeIndex - uint3(1, 0, 0), setup.voxelDimension.xyz)]) - float(voxelDensities[GetLinearIndex(voxelVolumeIndex + uint3(1, 0, 0), setup.voxelDimension.xyz)]);
            normal.y = float(voxelDensities[GetLinearIndex(voxelVolumeIndex - uint3(0, 1, 0), setup.voxelDimension.xyz)]) - float(voxelDensities[GetLinearIndex(voxelVolumeIndex + uint3(0, 1, 0), setup.voxelDimension.xyz)]);
            normal.z = float(voxelDensities[GetLinearIndex(voxelVolumeIndex - uint3(0, 0, 1), setup.voxelDimension.xyz)]) - float(voxelDensities[GetLinearIndex(voxelVolumeIndex + uint3(0, 0, 1), setup.voxelDimension.xyz)]);
            voxelNormals[i] = normalize(normal);
        }

        code |= (((cornerDensities[i] > setup.isovalue) ? 1 : 0) << i);
    }

    // Get global vertex indices from local vertex indices;
    uint x = cellVolumeIndex.x;
    uint y = cellVolumeIndex.y;
    uint z = cellVolumeIndex.z;
    uint DX = setup.cellDimension.x;
    uint DY = setup.cellDimension.y;
    uint DZ = setup.cellDimension.z;

    // Set global vertex indices
    uint globalVertexIndices[VERTICES_IN_CELL];

    // Voxels with the codes 0 or 255 are completely inside or outside the scalar field and thus trivial. 
    if (code != 0 && code != 255)
    {
        globalVertexIndices[0] = z * (DY + 1) * DX + y * DX + x;
        globalVertexIndices[2] = globalVertexIndices[0] + DX;
        globalVertexIndices[4] = globalVertexIndices[0] + (DY + 1) * DX;
        globalVertexIndices[6] = globalVertexIndices[0] + (DY + 2) * DX;

        // Vertices located on edges aligned with the y axis
        globalVertexIndices[3] = (DZ + 1) * (DY + 1) * DX + z * DY * (DX + 1) + y * (DX + 1) + x;
        globalVertexIndices[1] = globalVertexIndices[3] + 1;
        globalVertexIndices[7] = globalVertexIndices[3] + DY * (DX + 1);
        globalVertexIndices[5] = globalVertexIndices[3] + DY * (DX + 1) + 1;

        // Vertices located on edges aligned with the z axis
        globalVertexIndices[8] = (DZ + 1) * (DY + 1) * DX + (DZ + 1) * DY * (DX + 1) + z * (DY + 1) * (DX + 1) + y * (DX + 1) + x;
        globalVertexIndices[9] = globalVertexIndices[8] + 1;
        globalVertexIndices[11] = globalVertexIndices[8] + (DX + 1);
        globalVertexIndices[10] = globalVertexIndices[8] + (DX + 2);
    }

    float3 vertexPositionsInCell[INDICES_IN_CELL]; // This will be used to find normals later
    bool notLastEdge = (cellVolumeIndex.x != setup.cellDimension.x - 1) && (cellVolumeIndex.y != setup.cellDimension.y - 1) && (cellVolumeIndex.z != setup.cellDimension.z - 1);

    // Record vertex indices into IndexOutSSBO, vertex positions into VertexPositionOutSSBO, and normals into NormalOutSSBO
    // INDICES_IN_CELL == 5 as there are maximum five triangles in each cell
    // i: Triangle index
    // j: Vertex index in each triangle
    for (uint i = 0; i < INDICES_IN_CELL / 3; ++i)
    {
        // If there is another triangle
        if (indexTable[code * INDICES_IN_CELL + 3 * i] != -1)
        {
            uint currentIndexCount;
            InterlockedAdd(drawArguments[0].indexCount, 3, currentIndexCount);
            for (uint j = 0; j < 3; ++j)
            {
                uint offset = 3 * i + j; // Index of this vertex in the cell
                uint localVertexIndex = indexTable[code * INDICES_IN_CELL + offset]; // 0, 1, ..., 7
                uint globalVertexIndex = globalVertexIndices[localVertexIndex];

                // Output the result to the output index buffer.
                // Since we will process vertex 0, 3, 8 in each cell, the vertices on the last edge for each axis will be invalid.
                // Make triangles only for vertices not on the last edges.
                if (notLastEdge) indices[currentIndexCount + j] = globalVertexIndex;

                // Update the positions of vertices composing the triangle
                if (localVertexIndex == 0 || localVertexIndex == 3 || localVertexIndex == 8)
                {
                    // Store vertex positions in the vertex position buffer!
                    uint2 adjacentCorners = VERTEX_TO_CORNERS[localVertexIndex];
                    bool isLargerY = cornerDensities[adjacentCorners.x] < cornerDensities[adjacentCorners.y];

                    float density1 = isLargerY ? cornerDensities[adjacentCorners.x] : cornerDensities[adjacentCorners.y];
                    float density2 = isLargerY ? cornerDensities[adjacentCorners.y] : cornerDensities[adjacentCorners.x];
                    float weight1 = (density2 - setup.isovalue) / (density2 - density1);
                    float weight2 = (setup.isovalue - density1) / (density2 - density1);

                    float3 position1 = isLargerY ? voxelPositions[adjacentCorners.x] : voxelPositions[adjacentCorners.y];
                    float3 position2 = isLargerY ? voxelPositions[adjacentCorners.y] : voxelPositions[adjacentCorners.x];
                    float3 normal1 = isLargerY ? voxelNormals[adjacentCorners.x] : voxelNormals[adjacentCorners.y];
                    float3 normal2 = isLargerY ? voxelNormals[adjacentCorners.y] : voxelNormals[adjacentCorners.x];

                    vertices[globalVertexIndex].pos = weight1 * position1 + weight2 * position2;
                    vertices[globalVertexIndex].normal = weight1 * normal1 + weight2 * normal2;
                    vertices[globalVertexIndex].texCoord = float2(0.0f, 0.0f);
                }
            }
        }
    }
}
