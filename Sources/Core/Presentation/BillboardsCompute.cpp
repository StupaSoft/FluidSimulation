#include "BillboardsCompute.h"

BillboardsCompute::BillboardsCompute(const std::vector<Buffer> &inputBuffers, size_t particleCount, const Buffer &vertexOutputBuffer) :
	_particlePositionInputBuffers(inputBuffers),
	_particleCount(static_cast<uint32_t>(particleCount))
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_particleCountBuffer = CreateBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	memory->Bind({ _particleCountBuffer });
	_particleCountBuffer->CopyFrom(&_particleCount, 0);

	Shader computeShader = ShaderManager::Get()->GetShaderAsset("BillboardsPopulating");
	_populatingDescriptor = CreateDescriptors(computeShader, particleCount, _particleCountBuffer, _particlePositionInputBuffers, vertexOutputBuffer);
	_populatingPipeline = CreateComputePipeline(computeShader->GetShaderModule(), _populatingDescriptor->GetDescriptorSetLayout());
}

BillboardsCompute::~BillboardsCompute()
{
	vkDeviceWaitIdle(VulkanCore::Get()->GetLogicalDevice());
}

void BillboardsCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame)
{
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _populatingPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _populatingPipeline->GetPipelineLayout(), 0, 1, &_populatingDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, DivisionCeil(_particleCount, 1024), 1, 1);
}

Descriptor BillboardsCompute::CreateDescriptors(const Shader &shader, size_t particleCount, const Buffer &particleCountBuffer, const std::vector<Buffer> &particlePositionBuffers, const Buffer &vertexOutputBuffer)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("particleCount", particleCountBuffer);
	descriptor->BindBuffer("positions", particlePositionBuffers[0]);
	descriptor->BindBuffer("vertices", vertexOutputBuffer);

	auto descriptorSetLayout = descriptor->GetDescriptorSetLayout();
	auto descriptorSets = descriptor->GetDescriptorSets();

	return descriptor;
}
