#include "SimulationPanel.h"

SimulationPanel::SimulationPanel(const std::shared_ptr<SimulatedSceneBase> &simulatedScene) :
	_simulatedScene(simulatedScene)
{
	_simulatedScene->UpdateSimulationParameters(*_simulationParameters);
}

void SimulationPanel::Draw()
{
	ImGui::Begin("Simulation");

	bool parametersUpdated = false;
	parametersUpdated |= ImGui::SliderFloat("Time Step", &_simulationParameters->_timeStep, 0.001f, 0.1f);
	parametersUpdated |= ImGui::SliderFloat("Particle Mass", &_simulationParameters->_particleMass, 0.001f, 1.0f);
	parametersUpdated |= ImGui::SliderFloat("Target Density", &_simulationParameters->_targetDensity, 1.0f, 1000.0f);
	parametersUpdated |= ImGui::SliderFloat("Sound Speed", &_simulationParameters->_soundSpeed, 0.1f, 10.0f);
	parametersUpdated |= ImGui::SliderFloat("EOS Exponent", &_simulationParameters->_eosExponent, 0.1f, 10.0f);
	parametersUpdated |= ImGui::SliderFloat("Kernel Radius Factor", &_simulationParameters->_kernelRadiusFactor, 1.0f, 10.0f);
	parametersUpdated |= ImGui::SliderFloat("Drag Coefficient", &_simulationParameters->_dragCoefficient, 0.0001f, 1.0f);
	parametersUpdated |= ImGui::SliderFloat("Viscosity Coefficient", &_simulationParameters->_viscosityCoefficient, 0.0005f, 0.5f);
	parametersUpdated |= ImGui::SliderFloat("Restitution Coefficient", &_simulationParameters->_restitutionCoefficient, 0.1f, 1.0f);
	parametersUpdated |= ImGui::SliderFloat("Friction Coefficient", &_simulationParameters->_frictionCoefficient, 0.0f, 1.0f);

	if (ImGui::Button("Reset Simulation Parameters"))
	{
		_simulationParameters = std::make_shared<SimulationParameters>();
		parametersUpdated = true;
	}

	if (parametersUpdated) _simulatedScene->UpdateSimulationParameters(*_simulationParameters);

	if (ImGui::Button("Start Simulation"))
	{
		_simulatedScene->InitializeLevel();
		_simulatedScene->InitializeParticles(0.07f, { -1.0f, 1.0f }, { 2.0f, 6.0f }, { -1.0f, 1.0f }); // Temp
		//_simulatedScene->InitializeParticles(0.07f, { -0.8f, 0.8f }, { 1.0f, 4.0f }, { -0.8f, 0.8f }); // Temp
	}

	ImGui::End();
}
