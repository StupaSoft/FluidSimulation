#include "ModelBase.h"

void ModelBase::Register()
{
	_eventID = _vulkanCore->OnDrawCommand().AddListener
	(
		weak_from_this(),
		[this](VkCommandBuffer commandBuffer, size_t currentFrame)
		{
			RecordCommand(commandBuffer, currentFrame);
		}
	);
}
