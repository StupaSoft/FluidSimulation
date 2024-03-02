#include "SimulationPanel.h"

SimulationPanel::SimulationPanel(const std::shared_ptr<SimulatedSceneBase> &simulatedScene) :
	_simulatedScene(simulatedScene)
{
	_simulatedScene->UpdateSimulationParamters(*_simulationParameters);
}

void SimulationPanel::Draw()
{
	ImGui::Begin("Simulation");

	bool paramtersChanged = false;
	paramtersChanged |= ImGui::SliderFloat("Particle Mass", &_simulationParameters->_particleMass, 0.001f, 1.0f);
	paramtersChanged |= ImGui::SliderFloat("Target Density", &_simulationParameters->_targetDensity, 1.0f, 1000.0f);
	paramtersChanged |= ImGui::SliderFloat("Sound Speed", &_simulationParameters->_soundSpeed, 0.1f, 10.0f);
	paramtersChanged |= ImGui::SliderFloat("EOS Exponent", &_simulationParameters->_eosExponent, 0.1f, 10.0f);
	paramtersChanged |= ImGui::SliderFloat("Kernel Radius Factor", &_simulationParameters->_kernelRadiusFactor, 1.0f, 10.0f);
	paramtersChanged |= ImGui::SliderFloat("Drag Coefficient", &_simulationParameters->_dragCoefficient, 0.0001f, 1.0f);
	paramtersChanged |= ImGui::SliderFloat("Viscosity Coefficient", &_simulationParameters->_viscosityCoefficient, 0.0005f, 0.5f);
	paramtersChanged |= ImGui::SliderFloat("Restitution Coefficient", &_simulationParameters->_restitutionCoefficient, 0.1f, 1.0f);
	paramtersChanged |= ImGui::SliderFloat("Friction Coefficient", &_simulationParameters->_frictionCoefficient, 0.0f, 1.0f);

	if (ImGui::Button("Reset Simulation Parameters"))
	{
		_simulationParameters = std::make_unique<SimulationParameters>();
		paramtersChanged = true;
	}

	if (paramtersChanged) _simulatedScene->UpdateSimulationParamters(*_simulationParameters);

	if (ImGui::Button("Start Simulation"))
	{
		_simulatedScene->InitializeParticles(0.07f, { -2.3f, -0.4f }, { 1.0f, 4.0f }, { -1.4f, 1.4f }); // Temp
		//_simulatedScene->InitializeParticles(0.03f, 0.07f, { -0.8f, 0.8f }, { 1.0f, 4.0f }, { -0.8f, 0.8f }); // Temp
		_simulatedScene->SetPlay(true);
	}

	ImGui::End();
}
