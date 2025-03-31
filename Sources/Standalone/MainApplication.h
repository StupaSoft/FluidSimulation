#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <string>
#include <format>
#include <optional>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
#include <array>
#include <chrono>
#include <unordered_map>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "VulkanCore.h"

#include "SimulatedSceneBase.h"
#include "CPUSimulatedScene.h"
#include "GPUSimulatedScene.h"

#include "UIModel.h"
#include "SimulationPanel.h"
#include "RenderingPanel.h"
#include "MaterialPanel.h"

class WindowApplication
{
private:
	VulkanCore *_vulkanCore;
	std::shared_ptr<SimulatedSceneBase> _simulatedScene;

public:
	void Run();

private:
	GLFWwindow *_window;
	static const uint32_t INIT_WIDTH;
	static const uint32_t INIT_HEIGHT;

	GLFWwindow *InitMainWindow(int width, int height, const std::string &title);
	void MainLoop();

	void Resize();
	static void OnFramebufferResized(GLFWwindow *window, int width, int height);
};
