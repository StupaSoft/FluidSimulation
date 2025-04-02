import RenderModule;

struct Sphere
{
    float4 pos;
    float radius;
};

ConstantBuffer<uint2> screenSize;

ConstantBuffer<float> learningRate;
RWStructuredBuffer<Material> learningMaterial;
RWStructuredBuffer<Material> gradientMaterial;

[shader("compute")]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    float adjustedLearningRate = learningRate / (screenSize.x * screenSize.y);

    // Gradient descent: update the learning material with the gradient
    learningMaterial[0].color -= adjustedLearningRate * gradientMaterial[0].color;
    learningMaterial[0].specularColor -= adjustedLearningRate * gradientMaterial[0].specularColor;
    learningMaterial[0].glossiness -= adjustedLearningRate * gradientMaterial[0].glossiness;

    // Reset the gradient
    gradientMaterial[0].color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    gradientMaterial[0].specularColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    gradientMaterial[0].glossiness = 0.0f;
}

