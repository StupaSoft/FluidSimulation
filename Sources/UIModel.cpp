#include "UIModel.h"

UIModel::UIModel(const ModelInitInfo &modelInitInfo) :
	ModelBase(modelInitInfo)
{
	// Get ready
	_ImGuiDescriptorPool = CreateImGuiDescriptorPool();

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls

	ImGui::StyleColorsDark(); // Setup Dear ImGui style

	ImGui_ImplGlfw_InitForVulkan(_modelInitInfo._window, true);
	ImGui_ImplVulkan_InitInfo initInfo =
	{
		.Instance = _modelInitInfo._instance,
		.PhysicalDevice = _modelInitInfo._physicalDevice,
		.Device = _modelInitInfo._logicalDevice,
		.QueueFamily = _modelInitInfo._queueFamily,
		.Queue = _modelInitInfo._graphicsQueue,
		.PipelineCache = VK_NULL_HANDLE,
		.DescriptorPool = _ImGuiDescriptorPool,
		.MinImageCount = _modelInitInfo._minImageCount,
		.ImageCount = static_cast<uint32_t>(_modelInitInfo._swapChainImageCount),
		.MSAASamples = VK_SAMPLE_COUNT_8_BIT,
		.Allocator = nullptr,
		.CheckVkResultFn = nullptr
	};

	ImGui_ImplVulkan_Init(&initInfo, _modelInitInfo._renderPass);
}

void UIModel::OnCleanUpOthers()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyDescriptorPool(_modelInitInfo._logicalDevice, _ImGuiDescriptorPool, nullptr);
}

void UIModel::RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	// Dear ImGui
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame(); // Handle user imputs, the screen size etc.
	ImGui::NewFrame(); // Initialize an actual ImGuiFrame

	for (const auto &panel : _panels)
	{
		panel->Draw();
	}

	ImGui::Render();
	ImDrawData *mainDrawData = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(mainDrawData, commandBuffer); // Add draw calls to the render pass
}

VkDescriptorPool UIModel::CreateImGuiDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes = // Which descriptor types the descriptor sets are going to contain and how many of them?
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = _modelInitInfo._maxFramesInFlight,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(_modelInitInfo._logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a ImGui descriptor pool.");
	}

	return descriptorPool;
}
