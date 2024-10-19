#pragma once

#include "VulkanCore.h"
#include "BillboardsCompute.h"
#include "MeshModel.h"

class Billboards
{
private:
	size_t _particleCount = 0;

	std::shared_ptr<BillboardsCompute> _compute = nullptr;

	std::shared_ptr<MeshModel> _meshModel = nullptr;
	std::shared_ptr<MeshObject> _meshObject = nullptr;

	std::vector<Vertex> _billboardVertices;
	std::vector<uint32_t> _billboardIndices;

	Buffer _vertexBuffer = nullptr;
	Buffer _indexBuffer = nullptr;
	Buffer _drawArgumentBuffer = nullptr;

	// 3 2
	// 0 1
	const std::vector<glm::vec2> VERTICES_IN_PARTICLE{ { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } }; // (Camera right component, camera up component, 0.0f)
	const std::vector<uint32_t> INDICES_IN_PARTICLE{ 0, 1, 2, 0, 2, 3 };

public:
	Billboards(const std::vector<Buffer> &inputBuffers, size_t particleCount);

	void SetEnable(bool enable);

	void UpdateRadius(float particleRadius);

	BillboardsCompute *GetCompute() { return _compute.get(); }
	MeshObject *GetMeshObject() { return _meshObject.get(); }
};

