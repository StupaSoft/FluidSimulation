#include "RenderingPanel.h"

void RenderingPanel::Draw()
{
	ImGui::Begin("Rendering");

    std::vector<std::string> items = { "Particle", "Marching Cubes" };
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
                if (currentItem == "Particle")
                {
                    particleRenderingMode = ParticleRenderingMode::Particle;
                }
                else if (currentItem == "Marching Cubes")
                {
                    particleRenderingMode = ParticleRenderingMode::MarchingCubes;
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

	ImGui::End();
}
