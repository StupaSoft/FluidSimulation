#include "MeshObject.h"

MeshObject::MeshObject(const std::shared_ptr<VulkanCore> &vulkanCore) :
	_vulkanCore(vulkanCore)
{
	std::tie(_uniformBuffers, _uniformBuffersMemory) = CreateUniformBuffers();

	SetPosition(glm::vec3());
	SetRotation(glm::vec3());
}

std::vector<VkBuffer> MeshObject::GetUniformBuffers()
{
	return _uniformBuffers;
}

void MeshObject::CleanUp()
{
	for (size_t i = 0; i < _uniformBuffers.size(); ++i)
	{
		vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _uniformBuffers[i], nullptr);
		vkFreeMemory(_vulkanCore->GetLogicalDevice(), _uniformBuffersMemory[i], nullptr);
	}
}

void MeshObject::SetPosition(glm::vec3 position)
{
	_position = glm::translate(glm::mat4(1.0f), position);
	ApplyModelTransformation();
}

void MeshObject::Translate(glm::vec3 offset)
{
	_position = glm::translate(_position, offset);
	ApplyModelTransformation();
}

void MeshObject::SetRotation(glm::vec3 rotation)
{
	glm::mat4 rotationMat = glm::mat4(1.0f);
	rotationMat = glm::rotate(rotationMat, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationMat = glm::rotate(rotationMat, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationMat = glm::rotate(rotationMat, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	_rotation = std::move(rotationMat);
	ApplyModelTransformation();
}

void MeshObject::Rotate(glm::vec3 axis, float angle)
{
	_rotation = glm::rotate(_rotation, glm::radians(angle), axis);
	ApplyModelTransformation();
}

std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> MeshObject::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(MVPMatrix);

	std::vector<VkBuffer> uniformBuffers(_vulkanCore->GetMaxFramesInFlight());
	std::vector<VkDeviceMemory> uniformBuffersMemory(_vulkanCore->GetMaxFramesInFlight());

	for (size_t i = 0; i < _vulkanCore->GetMaxFramesInFlight(); ++i)
	{
		std::tie(uniformBuffers[i], uniformBuffersMemory[i]) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	return std::make_tuple(uniformBuffers, uniformBuffersMemory);
}

void MeshObject::ApplyModelTransformation()
{
	MVPMatrix matrix
	{
		._model = _rotation * _position
	};

	for (size_t i = 0; i < _vulkanCore->GetMaxFramesInFlight(); ++i)
	{
		auto copyOffset = 0;
		auto copySize = sizeof(MVPMatrix::_model);

		void *data;
		vkMapMemory(_vulkanCore->GetLogicalDevice(), _uniformBuffersMemory[i], copyOffset, copySize, 0, &data);
		memcpy(data, &matrix, copySize);
		vkUnmapMemory(_vulkanCore->GetLogicalDevice(), _uniformBuffersMemory[i]);
	}
}

void MeshObject::SetCameraTransformation(const glm::mat4 &view, const glm::mat4 &projection)
{
	MVPMatrix matrix
	{
		._view = view,
		._project = projection
	};

	for (size_t i = 0; i < _vulkanCore->GetMaxFramesInFlight(); ++i)
	{
		auto copyOffset = offsetof(MVPMatrix, _view);
		auto copySize = sizeof(matrix) - copyOffset;

		void *data;
		vkMapMemory(_vulkanCore->GetLogicalDevice(), _uniformBuffersMemory[i], copyOffset, copySize, 0, &data);
		memcpy(data, &matrix._view, copySize);
		vkUnmapMemory(_vulkanCore->GetLogicalDevice(), _uniformBuffersMemory[i]);
	}
}
