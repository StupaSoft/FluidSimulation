#pragma once

#include "ModelBase.h"
#include "PanelBase.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "VulkanCore.h"

class UIModel : public ModelBase
{
private:
	std::vector<std::shared_ptr<PanelBase>> _panels;

	VkDescriptorPool _ImGuiDescriptorPool;

public:
	explicit UIModel(const std::shared_ptr<VulkanCore> &vulkanCore);

	virtual void RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;

	template<typename TPanel, typename... TArgs>
	std::shared_ptr<TPanel> AddPanel(TArgs&&... args)
	{
		std::shared_ptr<TPanel> panel = std::make_shared<TPanel>(std::forward<TArgs>(args)...);
		_panels.push_back(panel);

		return panel;
	}

private:
	VkDescriptorPool CreateImGuiDescriptorPool();
};