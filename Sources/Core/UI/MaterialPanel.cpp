#include "MaterialPanel.h"

MaterialPanel::MaterialPanel(const std::shared_ptr<Material> &material):
	_material(material)
{

}

void MaterialPanel::Draw()
{
	ImGui::Begin("Material");

	float color[4] = { _material->_color.x, _material->_color.y, _material->_color.z, _material->_color.w };
	ImGui::ColorPicker4("Color", color);
	_material->_color = { color[0], color[1], color[2], color[3] };

	float specularColor[4] = { _material->_specularColor.x, _material->_specularColor.y, _material->_specularColor.z, _material->_specularColor.w };
	ImGui::ColorPicker4("Specular Color", specularColor);
	_material->_specularColor = { specularColor[0], specularColor[1], specularColor[2], specularColor[3]};

	ImGui::SliderFloat("Glossiness", &_material->_glossiness, 3.0f, 100.0f);

	ImGui::End();
}
