#pragma once

#include "ComputeBase.h"
#include "VulkanStructs.h"
#include "DescriptorHelper.h"

class BillboardsCompute : public ComputeBase
{
private:
	uint32_t _particleCount = 0;

	Buffer _particleCountBuffer{};
	std::vector<Buffer> _particlePositionInputBuffers;

	VkDescriptorPool _populatingDescriptorPool = VK_NULL_HANDLE;

	VkDescriptorSetLayout _populatingDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _populatingDescriptorSets;
	VkPipeline _populatingPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _populatingPipelineLayout = VK_NULL_HANDLE;

public:
	BillboardsCompute(const std::shared_ptr<VulkanCore> &vulkanCore, const std::vector<Buffer> &inputBuffers, size_t particleCount, const Buffer &vertexOutputBuffer);
	virtual ~BillboardsCompute();

	const auto &GetParticlePositionBuffers() { return _particlePositionInputBuffers; }

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) override;
	virtual uint32_t GetOrder() override { return 0; }

private:
	std::tuple<VkDescriptorPool, VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateDescriptors(size_t particleCount, const Buffer &particleCountBuffer, const std::vector<Buffer> &particlePositionBuffers, const Buffer &vertexOutputBuffer);
};

