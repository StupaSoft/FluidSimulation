#include "ModelBase.h"

ModelBase::ModelBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) 
{
	_eventID = _vulkanCore->OnDrawCommand().AddListener
	(
		[this](VkCommandBuffer commandBuffer, uint32_t currentFrame)
		{
			RecordCommand(commandBuffer, currentFrame);
		}
	);
}

ModelBase::~ModelBase()
{
	_vulkanCore->OnDrawCommand().RemoveListener(_eventID);
}
