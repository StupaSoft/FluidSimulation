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

	Buffer _screenBuffer = nullptr;
	Buffer _cameraBuffer = nullptr;
	Buffer _lightBuffer = nullptr;

	Buffer _targetSceneBuffer = nullptr;
	Buffer _renderedSceneBuffer = nullptr;

	Buffer _targetSphereBuffer = nullptr;
	Buffer _learningSphereBuffer = nullptr;

	Buffer _targetMaterialBuffer = nullptr;
	Buffer _learningMaterialBuffer = nullptr;

	Descriptor _rayTracingDescriptor = nullptr;
	Pipeline _rayTracingPipeline = nullptr;

	std::shared_ptr<Material> _material = std::make_shared<Material>();

	int _screenWidth = 0;
	int _screenHeight = 0;

public:
	RayTracerCompute(const Image &screen);
	virtual ~RayTracerCompute();
	auto GetMaterial() const { return _material; }

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) override;

private:
	Descriptor CreateDescriptors(const Shader &shader);
};

