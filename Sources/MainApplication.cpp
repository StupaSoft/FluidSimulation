#include "MainApplication.h"

const uint32_t WindowApplication::INIT_WIDTH = 1920;
const uint32_t WindowApplication::INIT_HEIGHT = 1080;

void WindowApplication::Run()
{
	_window = InitMainWindow(INIT_WIDTH, INIT_HEIGHT, "Fluid Simulation");

	_vulkanCore = std::make_shared<VulkanCore>(_window);
	_vulkanCore->InitVulkan();
	_vulkanCore->SetUpScene();

	_simulatedScene = std::make_shared<SimulatedScene>(_vulkanCore);
	_simulatedScene->AddProp("Models/Bath.obj", "", false, true); // Temp
	_simulatedScene->AddProp("Models/BathWireframe.obj", "", true, false); // Temp
	_simulatedScene->AddProp("Models/Obstacle.obj", "", false, true); // Temp
	_simulatedScene->AddProp("Models/ObstacleWireframe.obj", "", true, false);
	_simulatedScene->InitializeParticles(0.03f, 0.07f, {-1.3f, 0.7f}, {1.0f, 5.0f}, {-1.0f, 1.0f}); // Temp

	auto uiModel = _vulkanCore->AddModel<UIModel>();
	uiModel->AddPanel<SimulationPanel>(_simulatedScene);

	MainLoop();
}

GLFWwindow *WindowApplication::InitMainWindow(int width, int height, const std::string &title)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Do not create an OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

	glfwSetWindowUserPointer(window, this); // Store the pointer 'this' to pass to the callback function
	glfwSetFramebufferSizeCallback(window, OnFramebufferResized); // Add a callback function to execute when the window is resized

	return window;
}

void WindowApplication::MainLoop()
{
	// Application loop
	while (!glfwWindowShouldClose(_window))
	{
		glfwPollEvents();

		_simulatedScene->Update(0.002f);
		_vulkanCore->DrawFrame();
	}
}

void WindowApplication::Resize()
{
	_vulkanCore->Resize();
}

// Window resize callback
void WindowApplication::OnFramebufferResized(GLFWwindow *window, int width, int height)
{
	auto app = reinterpret_cast<WindowApplication *>(glfwGetWindowUserPointer(window));
	app->Resize();
}