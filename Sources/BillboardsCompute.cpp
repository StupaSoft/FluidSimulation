#include "BillboardsCompute.h"

BillboardsCompute::BillboardsCompute(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const Buffer &vertexOutputBuffer) :
	ComputeBase(vulkanCore),
	_particleCount(particleCount)
{
	_particleCountBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _particleCountBuffer, &particleCount, 0, sizeof(uint32_t));

	_particlePositionBuffers = CreateBuffers
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(glm::vec3) * particleCount,
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::tie(_populatingDescriptorPool, _populatingDescriptorSetLayout, _populatingDescriptorSets) = CreateDescriptors(particleCount, _particleCountBuffer, _particlePositionBuffers, vertexOutputBuffer);
	VkShaderModule computeShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile("Shaders/BillboardsPopulating.spv"));

	std::tie(_populatingPipeline, _populatingPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), computeShaderModule, _populatingDescriptorSetLayout);
}

BillboardsCompute::~BillboardsCompute()
{
	DestroyBuffers(_vulkanCore->GetLogicalDevice(), _particlePositionBuffers);
	vkDestroyDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), _populatingDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(_vulkanCore->GetLogicalDevice(), _populatingDescriptorPool, nullptr);

	vkDestroyPipelineLayout(_vulkanCore->GetLogicalDevice(), _populatingPipelineLayout, nullptr);
	vkDestroyPipeline(_vulkanCore->GetLogicalDevice(), _populatingPipeline, nullptr);
}

void BillboardsCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _populatingPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _populatingPipelineLayout, 0, 1, &_populatingDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_particleCount, 1024), 1, 1);
}

std::tuple<VkDescriptorPool, VkDescriptorSetLayout, std::vector<VkDescriptorSet>> BillboardsCompute::CreateDescriptors(size_t particleCount, const Buffer particleCountBuffer, const std::vector<Buffer> &particlePositionBuffers, const Buffer &vertexOutputBuffer)
{
	std::unique_ptr<DescriptorHelper> descriptorHelper = std::make_unique<DescriptorHelper>(_vulkanCore);

	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 });
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 });
	auto descriptorPool = descriptorHelper->GetDescriptorPool();

	BufferLayout particleCountLayout
	{
		.binding = 0,
		.dataSize = sizeof(uint32_t),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};

	BufferLayout particlePositionLayout
	{
		.binding = 1,
		.dataSize = sizeof(glm::vec3) * particleCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};
	BufferLayout vertexOutputLayout
	{
		.binding = 2,
		.dataSize = sizeof(Vertex) * particleCount * 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT
	};
	descriptorHelper->AddBufferLayout(particleCountLayout);
	descriptorHelper->AddBufferLayout(particlePositionLayout);
	descriptorHelper->AddBufferLayout(vertexOutputLayout);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	descriptorHelper->BindBuffer(0, particleCountBuffer);
	descriptorHelper->BindBuffers(1, particlePositionBuffers);
	descriptorHelper->BindBuffer(2, vertexOutputBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorPool, descriptorSetLayout, descriptorSets);
}
