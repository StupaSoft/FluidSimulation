#include "UIModel.h"

UIModel::UIModel()
{
	// Manually create a descriptor pool as we cannot directly set descriptor sizes with Descriptor class
	std::vector<VkDescriptorPoolSize> poolSizes
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

	VkDescriptorPoolCreateInfo poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1, // Only one descriptor set will be allocated
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}

	_ImGuiDescriptorPool = descriptorPool;

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
		.DescriptorPool = _ImGuiDescriptorPool,
		.RenderPass = VulkanCore::Get()->GetRenderPass(),
		.MinImageCount = VulkanCore::Get()->GetMinImageCount(),
		.ImageCount = static_cast<uint32_t>(VulkanCore::Get()->GetMinImageCount()),
		.MSAASamples = VK_SAMPLE_COUNT_8_BIT,
		.Allocator = nullptr,
		.CheckVkResultFn = nullptr
	};

	ImGui_ImplVulkan_Init(&initInfo);
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
