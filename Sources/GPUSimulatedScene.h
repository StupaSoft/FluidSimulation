#pragma once

#include "SimulatedSceneBase.h"
#include "SimulationCompute.h"

class GPUSimulatedScene : public SimulatedSceneBase
{
private:
	std::shared_ptr<SimulationCompute> _simulationCompute = nullptr;
	Buffer _particlePositionInputBuffer = nullptr;

public:
	GPUSimulatedScene();
	virtual void Register() override;
	virtual void InitializeLevel() override;
	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) override;
};