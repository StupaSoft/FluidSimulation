#pragma once

#include "VulkanCore.h"
#include "BillboardsCompute.h"
#include "MeshModel.h"

class Billboards
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

	std::unique_ptr<BillboardsCompute> _compute = nullptr;

	std::unique_ptr<MeshModel> _meshModel = nullptr;
	std::shared_ptr<MeshObject> _meshObject = nullptr;

	Buffer _vertexBuffer{};
	Buffer _indexBuffer{};

	// 3 2
	// 0 1
	const std::vector<glm::vec2> VERTICES_IN_PARTICLE{ { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } }; // (Camera right component, camera up component, 0.0f)
	const std::vector<uint32_t> INDICES_IN_PARTICLE{ 0, 1, 2, 0, 2, 3 };

public:
	Billboards(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, float particleRadius);

	void SetEnable(bool enable);

	BillboardsCompute *GetCompute() { return _compute.get(); }
	MeshObject *GetMeshObjecT() { return _meshObject.get(); }
};

