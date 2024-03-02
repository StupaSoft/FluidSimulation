#pragma once

#include "ComputeBase.h"
#include "VulkanStructs.h"
#include "DescriptorHelper.h"

class BillboardsCompute : public ComputeBase
{
private:
	size_t _particleCount = 0;

	Buffer _particleCountBuffer{};
	std::vector<Buffer> _particlePositionBuffers;

	VkDescriptorPool _populatingDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout _populatingDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _populatingDescriptorSets;

	VkPipeline _populatingPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _populatingPipelineLayout = VK_NULL_HANDLE;

public:
	BillboardsCompute(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const Buffer &vertexOutputBuffer);
	virtual ~BillboardsCompute();

	const auto &GetParticlePositionBuffers() { return _particlePositionBuffers; }

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, uint32_t currentFrame) override;
	virtual uint32_t GetOrder() override { return 0; }

private:
	std::tuple<VkDescriptorPool, VkDescriptorSetLayout, std::vector<VkDescriptorSet>> CreateDescriptors(size_t particleCount, const Buffer particleCountBuffer, const std::vector<Buffer> &particlePositionBuffers, const Buffer &vertexOutputBuffer);
};

