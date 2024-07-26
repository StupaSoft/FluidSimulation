#include "ComputeBase.h"

void ComputeBase::Register()
{
	SetEnable(true);
}

void ComputeBase::SetEnable(bool enable)
{
	if (enable)
	{
		_commandRegisterID = VulkanCore::Get()->OnComputeCommand().AddListener
		(
			weak_from_this(),
			[this](VkCommandBuffer computeCommandBuffer, size_t currentFrame)
			{
				RecordCommand(computeCommandBuffer, currentFrame);
			},
			PRIORITY_LOWEST,
			__FUNCTION__,
			__LINE__
		);
	}
	else
	{
		VulkanCore::Get()->OnComputeCommand().RemoveListener(_commandRegisterID);
	}
}
