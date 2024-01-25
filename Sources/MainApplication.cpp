#include "MainApplication.h"

const uint32_t WindowApplication::INIT_WIDTH = 1920;
const uint32_t WindowApplication::INIT_HEIGHT = 1080;

void WindowApplication::Run()
{
	_window = InitMainWindow(INIT_WIDTH, INIT_HEIGHT, "Fluid Simulation");

	_vulkanCore = std::make_shared<VulkanCore>(_window);
	_vulkanCore->InitVulkan();

	auto meshModel = _vulkanCore->AddModel<MeshModel>(); // Temp
	meshModel->LoadAssets("Models/M2A1.obj", "Textures/M2A1_diffuse.png", "Shaders/Vert.spv", "Shaders/Frag.spv");
	for (int i = 0; i < 10; ++i)
	{
		auto meshObject = meshModel->AddMeshObject();
		meshObject->Translate(glm::vec3(0.1f * i, 0.0f, 0.0f));
	}

	auto uiModel = _vulkanCore->AddModel<UIModel>();
	uiModel->AddPanel<LeftPanel>();

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
		auto prevTime = std::chrono::high_resolution_clock::now();

		glfwPollEvents();
		_vulkanCore->DrawFrame();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaSecond = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();

		std::cout << 1.0f / deltaSecond << std::endl;
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




