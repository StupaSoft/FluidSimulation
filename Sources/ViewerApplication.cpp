#include "ViewerApplication.h"

void MainApplication::Run()
{
	_window = InitWindow(WIDTH, HEIGHT, "Fluid Simulation");

	_vulkanCore = std::make_unique<VulkanCore>(_window);
	_vulkanCore->InitVulkan();
	_vulkanCore->InitImGui();

	MainLoop();
}

GLFWwindow *MainApplication::InitWindow(int width, int height, const std::string &title)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Do not create an OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

	glfwSetWindowUserPointer(window, this); // Store the pointer 'this' to pass to the callback function
	glfwSetFramebufferSizeCallback(window, OnFramebufferResized); // Add a callback function to execute when the window is resized

	return window;
}

void MainApplication::MainLoop()
{
	// Main window
	while (!glfwWindowShouldClose(_window))
	{
		glfwPollEvents();

		_vulkanCore->DrawFrame();
	}
}

void MainApplication::Resize()
{
	_vulkanCore->Resize();
}

// Window resize callback
void MainApplication::OnFramebufferResized(GLFWwindow *window, int width, int height)
{
	auto app = reinterpret_cast<MainApplication *>(glfwGetWindowUserPointer(window));
	app->Resize();
}




