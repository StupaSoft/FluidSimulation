module SimulationModule;

public static const uint OVERLAPPING_BUCKETS = 8;
public static const float PI = 3.141592;
public static const float EPSILON = 1e-10;
public static const float INF = 1.0f / 0.0f;

public struct SimulationSetup
{
    public uint particleCount;
}

public struct SimulationParameters
{
    public float particleRadius;
    public float particleMass;

    public float timeStep;

    public float4 gravitiy;

    public float targetDensity;
    public float soundSpeed;
    public float eosExponent;
    public float kernelRadiusFactor;

    public float dragCoefficient;
    public float viscosityCoefficient;
    public float restitutionCoefficient;
    public float frictionCoefficient;
}

public struct GridSetup
{
    public uint4 dimension;
}

public float FirstDerivative(float dist, float r1, float r4)
{
    float x = 1.0f - (dist / r1);
    return (((-45.0f) * x) * x) / (3.1415920257568359375f * r4);
}

public float SecondDerivative(float dist, float r1, float r5)
{
    float x = 1.0f - (dist / r1);
    return (90.0f * x) / (3.1415920257568359375f * r5);
}

public float3 Gradient(float dist, float3 directionToCenter, float r1, float r4)
{
    float param = dist;
    float param_1 = r1;
    float param_2 = r4;
    return directionToCenter * (-FirstDerivative(param, param_1, param_2));
}

public float Kernel(float dist, float r1, float r2, float r3)
{
	float x = 1.0f - dist * dist / r2;
	return (315.0f * x * x * x) / (64.0f * PI * r3);
}