import RenderModule;

ConstantBuffer<uint> particleCount;
RWStructuredBuffer<float3> positions;
RWStructuredBuffer<Vertex> vertices;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
    // An invocation deals with a single particle
    uint id = globalThreadID.x;
    if (id >= particleCount) return;

    vertices[id * 4 + 0].pos = positions[id];
    vertices[id * 4 + 1].pos = positions[id];
    vertices[id * 4 + 2].pos = positions[id];
    vertices[id * 4 + 3].pos = positions[id];
}