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
	virtual void Register() override;

	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) override;
	virtual void AddProp(const std::string &OBJPath, const std::string &texturePath = "", bool isVisible = true, bool isCollidable = true) override;
};