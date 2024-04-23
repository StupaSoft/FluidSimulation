#include "BillboardsCompute.h"

BillboardsCompute::BillboardsCompute(const std::shared_ptr<VulkanCore> &vulkanCore, const std::vector<Buffer> &inputBuffers, size_t particleCount, const Buffer &vertexOutputBuffer) :
	ComputeBase(vulkanCore),
	_particlePositionInputBuffers(inputBuffers),
	_particleCount(static_cast<uint32_t>(particleCount))
{
	_particleCountBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(uint32_t),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), &_particleCount, _particleCountBuffer, 0);

	std::tie(_populatingDescriptorPool, _populatingDescriptorSetLayout, _populatingDescriptorSets) = CreateDescriptors(particleCount, _particleCountBuffer, _particlePositionInputBuffers, vertexOutputBuffer);

	VkShaderModule computeShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile(L"Shaders/Presentation/BillboardsPopulating.spv"));
	std::tie(_populatingPipeline, _populatingPipelineLayout) = CreateComputePipeline(_vulkanCore->GetLogicalDevice(), computeShaderModule, _populatingDescriptorSetLayout);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), computeShaderModule, nullptr);
}

BillboardsCompute::~BillboardsCompute()
{
	vkDeviceWaitIdle(_vulkanCore->GetLogicalDevice());

	vkDestroyDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), _populatingDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(_vulkanCore->GetLogicalDevice(), _populatingDescriptorPool, nullptr);

	vkDestroyPipelineLayout(_vulkanCore->GetLogicalDevice(), _populatingPipelineLayout, nullptr);
	vkDestroyPipeline(_vulkanCore->GetLogicalDevice(), _populatingPipeline, nullptr);
}

void BillboardsCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame)
{
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _populatingPipeline);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _populatingPipelineLayout, 0, 1, &_populatingDescriptorSets[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_particleCount, 1024), 1, 1);
}

std::tuple<VkDescriptorPool, VkDescriptorSetLayout, std::vector<VkDescriptorSet>> BillboardsCompute::CreateDescriptors(size_t particleCount, const Buffer particleCountBuffer, const std::vector<Buffer> &particlePositionBuffers, const Buffer &vertexOutputBuffer)
{
	auto descriptorHelper = std::make_shared<DescriptorHelper>(_vulkanCore);

	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 });
	descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 });
	auto descriptorPool = descriptorHelper->GetDescriptorPool();

	descriptorHelper->AddBufferLayout(0, particleCountBuffer->_size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(1, particlePositionBuffers[0]->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorHelper->AddBufferLayout(2, vertexOutputBuffer->_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = descriptorHelper->GetDescriptorSetLayout();

	descriptorHelper->BindBuffer(0, particleCountBuffer);
	descriptorHelper->BindBuffers(1, particlePositionBuffers);
	descriptorHelper->BindBuffer(2, vertexOutputBuffer);
	auto descriptorSets = descriptorHelper->GetDescriptorSets();

	return std::make_tuple(descriptorPool, descriptorSetLayout, descriptorSets);
}
