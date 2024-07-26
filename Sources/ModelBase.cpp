#include "ModelBase.h"

void ModelBase::Register()
{
	_eventID = VulkanCore::Get()->OnDrawCommand().AddListener
	(
		weak_from_this(),
		[this](VkCommandBuffer commandBuffer, size_t currentFrame)
		{
			RecordCommand(commandBuffer, currentFrame);
		},
		_order
	);
}
