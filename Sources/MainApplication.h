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

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "VulkanCore.h"
#include "FileManager.h"

#include "MeshModel.h"
#include "UIModel.h"

#include "LeftPanel.h"

class WindowApplication
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore;

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
