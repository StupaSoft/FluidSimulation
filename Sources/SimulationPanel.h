#pragma once

#include <memory>

#include "PanelBase.h"
#include "CPUSimulatedScene.h"

class SimulationPanel : public PanelBase
{
private:
	std::shared_ptr<SimulatedSceneBase> _simulatedScene;
	std::shared_ptr<SimulationParameters> _simulationParameters = std::make_shared<SimulationParameters>();

public:
	SimulationPanel(const std::shared_ptr<SimulatedSceneBase> &simulatedScene);
	virtual void Draw() override;
};