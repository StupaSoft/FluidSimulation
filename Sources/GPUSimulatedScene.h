#pragma once

#include "SimulatedSceneBase.h"
#include "SimulationCompute.h"

class GPUSimulatedScene : public SimulatedSceneBase
{
private:
	std::shared_ptr<SimulationCompute> _simulationCompute = nullptr;
	Buffer _particlePositionInputBuffer = nullptr;

public:
	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) override;
};