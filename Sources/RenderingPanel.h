#pragma once

#include <memory>

#include "PanelBase.h"
#include "SimulatedScene.h"

class RenderingPanel : public PanelBase
{
private:
	static const std::string MARCHING_CUBES;
	static const std::string PARTICLE;

	std::shared_ptr<SimulatedScene> _simulatedScene;

public:
	RenderingPanel(const std::shared_ptr<SimulatedScene> &simulatedScene) : _simulatedScene(simulatedScene) {}
	virtual void Draw() override;
};