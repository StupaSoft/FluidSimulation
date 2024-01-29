#include "MainApplication.h"

const uint32_t WindowApplication::INIT_WIDTH = 1920;
const uint32_t WindowApplication::INIT_HEIGHT = 1080;

void WindowApplication::Run()
{
	_window = InitMainWindow(INIT_WIDTH, INIT_HEIGHT, "Fluid Simulation");

	_vulkanCore = std::make_shared<VulkanCore>(_window);
	_vulkanCore->InitVulkan();
	_vulkanCore->SetUpScene();

	auto uiModel = _vulkanCore->AddModel<UIModel>();
	uiModel->AddPanel<LeftPanel>();

	_simulatedScene = std::make_shared<SimulatedScene>(_vulkanCore);
	_simulatedScene->AddLevel("Models/M2A1.obj", "Textures/M2A1.png");  // Temp
	_simulatedScene->InitializeParticles(0.1f, 0.02f, 0.05f, {-2.0f, 2.0f}, {2.0f, 3.0f}, {-2.0f, 2.0f}); // Temp

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
	float deltaSecond = 0.01f;
	while (!glfwWindowShouldClose(_window))
	{
		auto prevTime = std::chrono::high_resolution_clock::now();

		glfwPollEvents();

		_simulatedScene->Update(deltaSecond);
		_vulkanCore->DrawFrame();

		auto currentTime = std::chrono::high_resolution_clock::now();
		deltaSecond = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
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