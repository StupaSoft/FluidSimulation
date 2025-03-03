#pragma once

#include "ModelBase.h"
#include "PanelBase.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "VulkanCore.h"
#include "Descriptor.h"

class UIModel : public ModelBase
{
private:
	std::vector<std::shared_ptr<PanelBase>> _panels;

	VkDescriptorPool _ImGuiDescriptorPool;

public:
	UIModel();
	virtual ~UIModel();

	virtual void RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame) override;

	template<typename TPanel, typename... TArgs>
	std::shared_ptr<TPanel> AddPanel(TArgs&&... args)
	{
		std::shared_ptr<TPanel> panel = PanelBase::Instantiate<TPanel>(std::forward<TArgs>(args)...);
		_panels.push_back(panel);

		return panel;
	}
};