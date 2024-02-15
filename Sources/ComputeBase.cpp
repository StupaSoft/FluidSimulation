#include "ComputeBase.h"

ComputeBase::ComputeBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) 
{
	_vulkanCore->OnComputeCommand().AddListener
	(
		[this](VkCommandBuffer commandBuffer, VkCommandBuffer computeCommandBuffer, uint32_t currentFrame)
		{
			RecordCommand(commandBuffer, computeCommandBuffer, currentFrame);
		}
	);
}