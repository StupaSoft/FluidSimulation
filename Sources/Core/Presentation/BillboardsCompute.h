#pragma once

#include "ComputeBase.h"
#include "VulkanResources.h"
#include "Descriptor.h"
#include "ShaderManager.h"

class BillboardsCompute : public ComputeBase
{
private:
	uint32_t _particleCount = 0;

	Buffer _particleCountBuffer{};
	std::vector<Buffer> _particlePositionInputBuffers;

	Descriptor _populatingDescriptor = nullptr;
	VkPipeline _populatingPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _populatingPipelineLayout = VK_NULL_HANDLE;

public:
	BillboardsCompute(const std::vector<Buffer> &inputBuffers, size_t particleCount, const Buffer &vertexOutputBuffer);
	virtual ~BillboardsCompute();

	const auto &GetParticlePositionBuffers() { return _particlePositionInputBuffers; }

protected:
	virtual void RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame) override;

private:
	Descriptor CreateDescriptors(const ShaderAsset &shader, size_t particleCount, const Buffer &particleCountBuffer, const std::vector<Buffer> &particlePositionBuffers, const Buffer &vertexOutputBuffer);
};

