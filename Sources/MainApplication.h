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

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "VulkanCore.h"
#include "FileManager.h"

class MainApplication
{
private:
	std::unique_ptr<VulkanCore> _vulkanCore;

public:
	void Run();

private:
	GLFWwindow *_window;
	const uint32_t WIDTH = 1920;
	const uint32_t HEIGHT = 1080;

	GLFWwindow *InitWindow(int width, int height, const std::string &title);
	void MainLoop();

	void Resize();
	static void OnFramebufferResized(GLFWwindow *window, int width, int height);
};
