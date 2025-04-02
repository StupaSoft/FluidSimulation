#pragma once

#include "ComputeBase.h"
#include "VulkanResources.h"
#include "Descriptor.h"
#include "ShaderManager.h"
#include "Pipeline.h"
#include "RenderParameters.h"

struct Sphere
{
	alignas(16) glm::vec4 pos;
	alignas(4) float radius;
};

class RayTracerCompute : public ComputeBase
{
private:
	Image _screen = nullptr;

	// Buffers
	Buffer _screenBuffer = nullptr;
	Buffer _cameraBuffer = nullptr;
	Buffer _lightBuffer = nullptr;

	Buffer _targetSceneBuffer = nullptr;
	Buffer _renderedSceneBuffer = nullptr;

	Buffer _targetSphereBuffer = nullptr;
	Buffer _learningSphereBuffer = nullptr;
	Buffer _gradientBuffer = nullptr;

	Buffer _targetMaterialBuffer = nullptr;
	Buffer _learningMaterialBuffer = nullptr;

	Buffer _learningRateBuffer = nullptr;

	// Pipelines
	Descriptor _rayTracingDescriptor = nullptr;
	Pipeline _rayTracingPipeline = nullptr;

	Descriptor _updaterDescriptor = nullptr;
	Pipeline _updaterPipeline = nullptr;

	Material _material{};

	float _learningRate = 0.3f;

	int _screenWidth = 0;
	int _screenHeight = 0;

public:
	RayTracerCompute(const Image &screen);
	virtual ~RayTracerCompute();
	auto &GetMaterial() { return _material; }

	float GetLearningRate() { return _learningRate; }
	void SetLearningRate(float learningRate);

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) override;

private:
	Descriptor CreateRayTracingDescriptors(const Shader &shader);
	Descriptor CreateUpdaterDescriptors(const Shader &shader);
};

