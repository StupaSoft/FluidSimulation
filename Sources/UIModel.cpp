#include "UIModel.h"

UIModel::UIModel(const std::shared_ptr<VulkanCore> &vulkanCore) :
	ModelBase(vulkanCore)
{
	// Get ready
	DescriptorHelper descriptorHelper(_vulkanCore);
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 });
	descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 });
	_ImGuiDescriptorPool = descriptorHelper.GetDescriptorPool();

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls

	ImGui::StyleColorsDark(); // Setup Dear ImGui style

	ImGui_ImplGlfw_InitForVulkan(_vulkanCore->GetWindow(), true);
	ImGui_ImplVulkan_InitInfo initInfo =
	{
		.Instance = _vulkanCore->GetInstance(),
		.PhysicalDevice = _vulkanCore->GetPhysicalDevice(),
		.Device = _vulkanCore->GetLogicalDevice(),
		.QueueFamily = _vulkanCore->GetGraphicsFamily(),
		.Queue = _vulkanCore->GetGraphicsQueue(),
		.PipelineCache = VK_NULL_HANDLE,
		.DescriptorPool = _ImGuiDescriptorPool,
		.MinImageCount = _vulkanCore->GetMinImageCount(),
		.ImageCount = static_cast<uint32_t>(_vulkanCore->GetMinImageCount()),
		.MSAASamples = VK_SAMPLE_COUNT_8_BIT,
		.Allocator = nullptr,
		.CheckVkResultFn = nullptr
	};

	ImGui_ImplVulkan_Init(&initInfo, _vulkanCore->GetRenderPass());
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

UIModel::~UIModel()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyDescriptorPool(_vulkanCore->GetLogicalDevice(), _ImGuiDescriptorPool, nullptr);
}
