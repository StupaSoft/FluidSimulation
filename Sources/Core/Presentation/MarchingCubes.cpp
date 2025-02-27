#include "MarchingCubes.h"

MarchingCubes::MarchingCubes(const std::vector<Buffer> &inputBuffers, size_t particleCount, const MarchingCubesGrid &marchingCubesGrid)
{
	_compute = MarchingCubesCompute::Instantiate<MarchingCubesCompute>(inputBuffers, particleCount, marchingCubesGrid);

	_meshModel = MeshModel::Instantiate<MeshModel>();
	_meshModel->LoadMesh(_compute->GetVertexBuffer(), _compute->GetIndexBuffer(), _compute->GetDrawArgumentBuffer());
	_meshModel->LoadPipeline("StandardVertexFragment", "StandardVertexFragment", "VSMain", "PSMain");

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
