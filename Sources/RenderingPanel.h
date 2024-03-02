#pragma once

#include <memory>

#include "PanelBase.h"
#include "CPUSimulatedScene.h"

class RenderingPanel : public PanelBase
{
private:
	static const std::string MARCHING_CUBES;
	static const std::string PARTICLE;

	std::shared_ptr<SimulatedSceneBase> _simulatedScene;

public:
	RenderingPanel(const std::shared_ptr<SimulatedSceneBase> &simulatedScene) : _simulatedScene(simulatedScene) {}
	virtual void Draw() override;
};