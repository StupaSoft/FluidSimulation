#include "MeshObject.h"

MeshObject::MeshObject(const ModelInitInfo &modelInitInfo) :
	_modelInitInfo(modelInitInfo)
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
		vkDestroyBuffer(_modelInitInfo._logicalDevice, _uniformBuffers[i], nullptr);
		vkFreeMemory(_modelInitInfo._logicalDevice, _uniformBuffersMemory[i], nullptr);
	}
}

void MeshObject::SetPosition(glm::vec3 position)
{
	_position = glm::translate(glm::mat4(1.0f), position);
	ApplyTransformations();
}

void MeshObject::Translate(glm::vec3 offset)
{
	_position = glm::translate(_position, offset);
	ApplyTransformations();
}

void MeshObject::SetRotation(glm::vec3 rotation)
{
	glm::mat4 rotationMat = glm::mat4(1.0f);
	rotationMat = glm::rotate(rotationMat, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationMat = glm::rotate(rotationMat, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationMat = glm::rotate(rotationMat, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	_rotation = std::move(rotationMat);
	ApplyTransformations();
}

void MeshObject::Rotate(glm::vec3 axis, float angle)
{
	_rotation = glm::rotate(_rotation, glm::radians(angle), axis);
	ApplyTransformations();
}

std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> MeshObject::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	std::vector<VkBuffer> uniformBuffers(_modelInitInfo._maxFramesInFlight);
	std::vector<VkDeviceMemory> uniformBuffersMemory(_modelInitInfo._maxFramesInFlight);

	for (size_t i = 0; i < _modelInitInfo._maxFramesInFlight; ++i)
	{
		std::tie(uniformBuffers[i], uniformBuffersMemory[i]) = CreateBuffer(_modelInitInfo._physicalDevice, _modelInitInfo._logicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	return std::make_tuple(uniformBuffers, uniformBuffersMemory);
}

void MeshObject::ApplyTransformations()
{
	for (size_t i = 0; i < _modelInitInfo._maxFramesInFlight; ++i)
	{
		UniformBufferObject ubo =
		{
			.model = _rotation * _position, // Identity matrix * rotation, axis
			.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // Eye position, target center position, up axis
			.proj = glm::perspective(glm::radians(45.0f), _modelInitInfo._swapChainExtent.width / (float)_modelInitInfo._swapChainExtent.height, 0.1f, 10.0f) // Vertical field of view, aspect ratio, clipping planes
		};

		// glm was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted.
		// Compensate this inversion.
		ubo.proj[1][1] *= -1;

		void *data;
		vkMapMemory(_modelInitInfo._logicalDevice, _uniformBuffersMemory[i], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(_modelInitInfo._logicalDevice, _uniformBuffersMemory[i]);
	}
}
