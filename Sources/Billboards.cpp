#include "Billboards.h"

Billboards::Billboards(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, float particleRadius) :
	_vulkanCore(vulkanCore)
{
	uint32_t vertexCount = static_cast<uint32_t>(particleCount * VERTICES_IN_PARTICLE.size());
	uint32_t indexCount = static_cast<uint32_t>(particleCount * INDICES_IN_PARTICLE.size());

	// Buffers
	_vertexBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		static_cast<uint32_t>(sizeof(Vertex) * vertexCount),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	_indexBuffer = CreateBuffer
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		static_cast<uint32_t>(sizeof(uint32_t) * indexCount),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	std::vector<Vertex> billboardVertices(vertexCount);
	std::vector<uint32_t> billboardIndices(indexCount);
	for (size_t i = 0; i < particleCount; ++i)
	{
		// Set attributes within each particle
		for (size_t j = 0; j < VERTICES_IN_PARTICLE.size(); ++j)
		{
			billboardVertices[i * VERTICES_IN_PARTICLE.size() + j].normal.x = particleRadius;
			billboardVertices[i * VERTICES_IN_PARTICLE.size() + j].texCoord = VERTICES_IN_PARTICLE[j];
		}

		// Set global indices of triangles
		for (size_t j = 0; j < INDICES_IN_PARTICLE.size(); ++j)
		{
			// First particle: 0, 1, 2, 0, 2, 3
			// Second particle: 4, 5, 6, 4, 6, 7
			// ...
			billboardIndices[i * INDICES_IN_PARTICLE.size() + j] = static_cast<uint32_t>(i * VERTICES_IN_PARTICLE.size() + INDICES_IN_PARTICLE[j]);
		}
	}

	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _vertexBuffer, billboardVertices.data(), 0, sizeof(Vertex) * vertexCount);
	CopyMemoryToBuffer(_vulkanCore->GetLogicalDevice(), _indexBuffer, billboardIndices.data(), 0, sizeof(uint32_t) * indexCount);

	// Cmpute
	_compute = std::make_unique<BillboardsCompute>(vulkanCore, particleCount, _vertexBuffer);

	// Presentation mesh
	_meshModel = std::make_unique<MeshModel>(_vulkanCore);
	_meshModel->LoadShaders("Shaders/ParticleVertex.spv", "Shaders/ParticleFragment.spv");
	_meshModel->SetMeshBuffers(_vertexBuffer, _indexBuffer, indexCount);

	MeshModel::Material particleMat
	{
		._color = glm::vec4(0.0f, 0.2f, 1.0f, 1.0f),
		._glossiness = 1.0f
	};
	_meshModel->SetMaterial(std::move(particleMat));
	_meshObject = _meshModel->AddMeshObject();
}

void Billboards::SetEnable(bool enable)
{
	_compute->SetEnable(enable);
	_meshObject->SetVisible(enable);
}
