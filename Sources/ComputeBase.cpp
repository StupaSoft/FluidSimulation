#include "ComputeBase.h"

ComputeBase::ComputeBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) 
{
}

void ComputeBase::Register()
{
	SetEnable(true);
}

void ComputeBase::SetEnable(bool enable)
{
	if (enable)
	{
		_commandRegisterID = _vulkanCore->OnComputeCommand().AddListener
		(
			weak_from_this(),
			[this](VkCommandBuffer computeCommandBuffer, size_t currentFrame)
			{
				RecordCommand(computeCommandBuffer, currentFrame);
			},
			__FUNCTION__,
			__LINE__
		);
	}
	else
	{
		_vulkanCore->OnComputeCommand().RemoveListener(_commandRegisterID);
	}
}
