#include "SimulationPanel.h"

void SimulationPanel::Draw()
{
	auto &simulationParameters = _simulatedScene->GetSimulationParameters();

	ImGui::SliderFloat("Particle Mass", &simulationParameters._particleMass, 0.001f, 1.0f);
	ImGui::SliderFloat("Target Density", &simulationParameters._targetDensity, 1.0f, 1000.0f);
	ImGui::SliderFloat("Sound Speed", &simulationParameters._soundSpeed, 0.1f, 10.0f);
	ImGui::SliderFloat("EOS Exponent", &simulationParameters._eosExponent, 0.1f, 10.0f);
	ImGui::SliderFloat("Kernel Radius Factor", &simulationParameters._kernelRadiusFactor, 1.0f, 10.0f);
	ImGui::SliderFloat("Drag Coefficient", &simulationParameters._dragCoefficient, 0.0001f, 1.0f);
	ImGui::SliderFloat("Viscosity Coefficient", &simulationParameters._viscosityCoefficient, 0.0001f, 1.0f);
	ImGui::SliderFloat("Restitution Coefficient", &simulationParameters._restitutionCoefficient, 0.1f, 1.0f);
	ImGui::SliderFloat("Friction Coefficient", &simulationParameters._frictionCoefficient, 0.0f, 1.0f);

	if (ImGui::Button("Start Simulation"))
	{
		_simulatedScene->InitializeParticles(0.03f, 0.07f, { -1.3f, 0.7f }, { 1.0f, 5.0f }, { -1.0f, 1.0f }); // Temp
		_simulatedScene->SetPlay(true);
	}
}
