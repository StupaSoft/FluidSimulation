#include "MainApplication.h"

const uint32_t WindowApplication::INIT_WIDTH = 1920;
const uint32_t WindowApplication::INIT_HEIGHT = 1080;

void WindowApplication::Run()
{
	_window = InitMainWindow(INIT_WIDTH, INIT_HEIGHT, "Fluid Simulation");

	_vulkanCore = std::make_shared<VulkanCore>(_window);
	_vulkanCore->InitVulkan();
	_vulkanCore->SetUpScene();

	//_simulatedScene = CPUSimulatedScene::Instantiate<CPUSimulatedScene>(_vulkanCore);
	_simulatedScene = GPUSimulatedScene::Instantiate<GPUSimulatedScene>(_vulkanCore);
	//_simulatedScene->AddProp("Models/Hemisphere.obj", "", true, true);
	//_simulatedScene->AddProp("Models/Filter.obj", "", true, true);
	//_simulatedScene->AddProp("Models/Bath.obj", "", true, true, RenderMode::Wireframe); // Temp
	//_simulatedScene->AddProp("Models/Obstacle.obj", "", true, true, RenderMode::Wireframe); // Temp
	_simulatedScene->AddProp("Models/Rocky.obj", "Textures/Brown.png", true, true);

	auto interfaceModel = UIModel::Instantiate<UIModel>(_vulkanCore);
	interfaceModel->AddPanel<SimulationPanel>(_simulatedScene);
	interfaceModel->AddPanel<RenderingPanel>(_simulatedScene);

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
	float deltaSecond = 0.0f;
	while (!glfwWindowShouldClose(_window))
	{
		auto prevTime = std::chrono::high_resolution_clock::now();

		glfwPollEvents();
		_vulkanCore->UpdateFrame(deltaSecond);

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