#pragma once

#include <memory>

#include "PanelBase.h"
#include "SimulatedScene.h"

class SimulationPanel : public PanelBase
{
private:
	std::shared_ptr<SimulatedScene> _simulatedScene;

public:
	SimulationPanel(const std::shared_ptr<SimulatedScene> &simulatedScene) : _simulatedScene(simulatedScene) {}
	virtual void Draw() override;
};