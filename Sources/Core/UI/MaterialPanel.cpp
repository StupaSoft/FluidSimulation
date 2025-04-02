#include "MaterialPanel.h"

MaterialPanel::MaterialPanel(const std::shared_ptr<RayTracerCompute> &rayTracer):
	_rayTracer(rayTracer)
{

}

void MaterialPanel::Draw()
{
	ImGui::Begin("Material");

	Material &material = _rayTracer->GetMaterial();
	float color[4] = { material._color.x, material._color.y, material._color.z, material._color.w };
	ImGui::ColorPicker4("Color", color);
	material._color = { color[0], color[1], color[2], color[3] };

	float specularColor[4] = { material._specularColor.x, material._specularColor.y, material._specularColor.z, material._specularColor.w };
	ImGui::ColorPicker4("Specular Color", specularColor);
	material._specularColor = { specularColor[0], specularColor[1], specularColor[2], specularColor[3]};

	ImGui::SliderFloat("Glossiness", &material._glossiness, 3.0f, 100.0f);

	float learningRate = _rayTracer->GetLearningRate();
	ImGui::SliderFloat("Learning Rate", &learningRate, 0.0f, 1.0f);
	_rayTracer->SetLearningRate(learningRate);

	ImGui::End();
}
