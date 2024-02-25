#include "ComputeBase.h"

ComputeBase::ComputeBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) 
{
	SetEnable(true);
}

ComputeBase::~ComputeBase()
{
	SetEnable(false);
}

void ComputeBase::SetEnable(bool enable)
{
	if (enable)
	{
		_eventID = _vulkanCore->OnComputeCommand().AddListener
		(
			[this](VkCommandBuffer computeCommandBuffer, uint32_t currentFrame)
			{
				RecordCommand(computeCommandBuffer, currentFrame);
			}
		);
	}
	else
	{
		_vulkanCore->OnComputeCommand().RemoveListener(_eventID);
	}
}
