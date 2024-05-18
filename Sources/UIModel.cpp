#include "UIModel.h"

UIModel::UIModel()
{
	// Get ready
	DescriptorHelper descriptorHelper{};
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

	ImGui_ImplGlfw_InitForVulkan(VulkanCore::Get()->GetWindow(), true);
	ImGui_ImplVulkan_InitInfo initInfo =
	{
		.Instance = VulkanCore::Get()->GetInstance(),
		.PhysicalDevice = VulkanCore::Get()->GetPhysicalDevice(),
		.Device = VulkanCore::Get()->GetLogicalDevice(),
		.QueueFamily = VulkanCore::Get()->GetGraphicsFamily(),
		.Queue = VulkanCore::Get()->GetGraphicsQueue(),
		.PipelineCache = VK_NULL_HANDLE,
		.DescriptorPool = _ImGuiDescriptorPool,
		.MinImageCount = VulkanCore::Get()->GetMinImageCount(),
		.ImageCount = static_cast<uint32_t>(VulkanCore::Get()->GetMinImageCount()),
		.MSAASamples = VK_SAMPLE_COUNT_8_BIT,
		.Allocator = nullptr,
		.CheckVkResultFn = nullptr
	};

	ImGui_ImplVulkan_Init(&initInfo, VulkanCore::Get()->GetRenderPass());
}

void UIModel::RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame)
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

	vkDestroyDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), _ImGuiDescriptorPool, nullptr);
}
