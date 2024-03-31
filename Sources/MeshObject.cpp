#include "MeshObject.h"

MeshObject::MeshObject(const std::shared_ptr<VulkanCore> &vulkanCore, const std::shared_ptr<std::vector<Triangle>> &triangles) :
	_vulkanCore(vulkanCore),
	_triangles(triangles)
{
	_mvpBuffers = CreateBuffers
	(
		_vulkanCore->GetPhysicalDevice(), 
		_vulkanCore->GetLogicalDevice(), 
		sizeof(MVP),
		_vulkanCore->GetMaxFramesInFlight(), 
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	SetPosition(glm::vec3());
	SetRotation(glm::vec3());
}

void MeshObject::Register()
{
	_vulkanCore->OnRecreateSwapChain().AddListener
	(
		weak_from_this(),
		[this]()
		{
			ApplyModelTransformation();
		}
	);

	auto &mainCamera = _vulkanCore->GetMainCamera();
	SetCameraTransformation(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());
	mainCamera->OnChanged().AddListener
	(
		weak_from_this(),
		[this](const Camera &camera)
		{
			SetCameraTransformation(camera.GetViewMatrix(), camera.GetProjectionMatrix());
		}
	);
}

std::vector<Buffer> MeshObject::GetMVPBuffers()
{
	return _mvpBuffers;
}

void MeshObject::CleanUp()
{
}

void MeshObject::SetPosition(glm::vec3 position)
{
	_translation = glm::translate(glm::mat4(1.0f), position);
	ApplyModelTransformation();
}

void MeshObject::Translate(glm::vec3 offset)
{
	_translation = glm::translate(_translation, offset);
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

void MeshObject::SetScale(glm::vec3 scale)
{
	_scale = glm::scale(glm::mat4(1.0f), scale);
	ApplyModelTransformation();
}

void MeshObject::Rotate(glm::vec3 axis, float angle)
{
	_rotation = glm::rotate(_rotation, glm::radians(angle), axis);
	ApplyModelTransformation();
}

void MeshObject::Scale(glm::vec3 scale)
{
	_scale = glm::scale(_scale, scale);
	ApplyModelTransformation();
}

void MeshObject::ApplyModelTransformation()
{
	MVP mvp
	{
		._model = _translation * _rotation * _scale
	};

	auto copyOffset = 0;
	auto copySize = sizeof(MVP::_model);
	CopyMemoryToBuffers(_vulkanCore->GetLogicalDevice(), &mvp, _mvpBuffers, copyOffset, copySize);

	UpdateWorldTriangles(mvp._model);
}

void MeshObject::SetCameraTransformation(const glm::mat4 &view, const glm::mat4 &projection)
{
	MVP mvp
	{
		._view = view,
		._projection = projection
	};

	auto copyOffset = offsetof(MVP, _view);
	auto copySize = sizeof(MVP) - copyOffset;
	CopyMemoryToBuffers(_vulkanCore->GetLogicalDevice(), &mvp, _mvpBuffers, copyOffset, copySize);
}

void MeshObject::UpdateWorldTriangles(const glm::mat4 &model)
{
	size_t triangleCount = _triangles->size();
	_worldTriangles.resize(triangleCount);

	auto modelInverse = glm::inverse(model);

	#pragma omp parallel for
	for (size_t i = 0; i < triangleCount; ++i)
	{
		const auto &triangle = _triangles->at(i);
		auto &worldTriangle = _worldTriangles[i];

		worldTriangle.A = model * triangle.A;
		worldTriangle.B = model * triangle.B;
		worldTriangle.C = model * triangle.C;

		worldTriangle.normalA = triangle.normalA * modelInverse;
		worldTriangle.normalB = triangle.normalB * modelInverse;
		worldTriangle.normalC = triangle.normalC * modelInverse;
	}
}
