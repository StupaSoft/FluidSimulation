#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "Delegate.h"

class PanelBase : public DelegateRegistrable<PanelBase>
{
public:
	virtual void Draw() = 0;
};