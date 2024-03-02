#pragma once

#include <memory>

#include "PanelBase.h"
#include "CPUSimulatedScene.h"

class SimulationPanel : public PanelBase
{
private:
	std::shared_ptr<SimulatedSceneBase> _simulatedScene;
	std::unique_ptr<SimulationParameters> _simulationParameters = std::make_unique<SimulationParameters>();

public:
	SimulationPanel(const std::shared_ptr<SimulatedSceneBase> &simulatedScene);
	virtual void Draw() override;
};