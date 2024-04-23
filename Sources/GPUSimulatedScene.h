#pragma once

#include "SimulatedSceneBase.h"
#include "SimulationCompute.h"

class GPUSimulatedScene : public SimulatedSceneBase
{
private:
	std::shared_ptr<SimulationCompute> _simulationCompute = nullptr;
	Buffer _particlePositionInputBuffer = nullptr;

public:
	GPUSimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore);

	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) override;
	virtual void AddProp(const std::wstring &OBJPath, const std::wstring &texturePath = L"", bool isVisible = true, bool isCollidable = true, RenderMode renderMode = RenderMode::Triangle) override;
};