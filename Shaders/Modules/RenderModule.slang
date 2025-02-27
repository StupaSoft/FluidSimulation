module RenderModule;

public struct MVP
{
    public float4x4 model;
    public float4x4 view;
    public float4x4 proj;
}

public struct Light
{
    public float4 lightDirection;
    public float4 lightColor;
    public float lightIntensity;
}

public struct Material
{
    public float4 materialColor;
    public float4 materialSpecularColor;
    public float materialGlossiness;
}

public struct VertexInput
{
    public float3 position;
    public float3 normal;
    public float2 texCoord;
}

public struct Vertex
{
	public float3 pos;
	public float3 normal;
	public float2 texCoord;
}