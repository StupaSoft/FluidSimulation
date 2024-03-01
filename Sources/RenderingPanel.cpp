#include "RenderingPanel.h"

const std::string RenderingPanel::MARCHING_CUBES = "Marching Cubes";
const std::string RenderingPanel::PARTICLE = "Particle";

void RenderingPanel::Draw()
{
	ImGui::Begin("Rendering");

    std::vector<std::string> items = { MARCHING_CUBES, PARTICLE };
    static std::string currentItem = items[0];
    if (ImGui::BeginCombo("Rendering Method", currentItem.data()))
    {
        for (int i = 0; i < items.size(); ++i)
        {
            bool isSelected = (currentItem == items[i]);

            if (ImGui::Selectable(items[i].data(), isSelected))
            {
                currentItem = items[i];

                ParticleRenderingMode particleRenderingMode = ParticleRenderingMode::Particle;
                if (currentItem == MARCHING_CUBES)
                {
                    particleRenderingMode = ParticleRenderingMode::MarchingCubes;
                }
                else if (currentItem == PARTICLE)
                {
                    particleRenderingMode = ParticleRenderingMode::Particle;
                }

                _simulatedScene->SetParticleRenderingMode(particleRenderingMode);
            }

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    if (currentItem == MARCHING_CUBES)
    {
        auto marchingCubes = _simulatedScene->GetMarchingCubes();
        if (marchingCubes != nullptr)
        {
            float isovalue = marchingCubes->GetCompute()->GetIsovalue();
            if (ImGui::SliderFloat("Isovalue", &isovalue, 0.0f, 5000.0f))
            {
                marchingCubes->GetCompute()->SetIsovalue(isovalue);
            }
        }
    }

	ImGui::End();
}
