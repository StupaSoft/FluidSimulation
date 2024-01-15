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

class WindowApplication
{
private:
	std::unique_ptr<VulkanCore> _vulkanCore;

public:
	void Run();

private:
	GLFWwindow *_window;
	const uint32_t WIDTH = 1920;
	const uint32_t HEIGHT = 1080;

	GLFWwindow *InitMainWindow(int width, int height, const std::string &title);
	void MainLoop();

	void Resize();
	static void OnFramebufferResized(GLFWwindow *window, int width, int height);
};
