module RenderModule;

public struct MVP
{
    public float4x4 model;
    public float4x4 view;
    public float4x4 proj;
}

public struct Light : IDifferentiable
{
    public float4 direction;
    public float4 color;
    public float intensity;
}

public struct Material : IDifferentiable
{
    public float4 color;
    public float4 specularColor;
    public float glossiness;
}

public struct VertexInput
{
    public float3 position;
    public float3 normal;
    public float2 texCoord;
}