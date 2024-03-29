#include "MarchingCubes.h"

MarchingCubes::MarchingCubes(const std::shared_ptr<VulkanCore> &vulkanCore, const std::vector<Buffer> &inputBuffers, size_t particleCount, const MarchingCubesGrid &marchingCubesGrid) :
	_vulkanCore(vulkanCore)
{
	_compute = MarchingCubesCompute::Instantiate<MarchingCubesCompute>(_vulkanCore, inputBuffers, particleCount, marchingCubesGrid);

	_meshModel = MeshModel::Instantiate<MeshModel>(_vulkanCore);
	_meshModel->SetMeshBuffers(_compute->GetVertexBuffer(), _compute->GetIndexBuffer(), _compute->GetIndexCount());
	_meshModel->LoadShaders("Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv");

	MeshModel::Material marchingCubesMat
	{
		._color = glm::vec4(0.0f, 0.2f, 1.0f, 1.0f),
		._glossiness = 1.0f
	};
	_meshModel->SetMaterial(std::move(marchingCubesMat));

	_meshObject = _meshModel->AddMeshObject();
}

void MarchingCubes::SetEnable(bool enable)
{
	_compute->SetEnable(enable);
	_meshObject->SetVisible(enable);
}
