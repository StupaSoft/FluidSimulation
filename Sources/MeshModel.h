#pragma once

#include "ModelBase.h"
#include "MeshObject.h"
#include "DescriptorHelper.h"
#include "Vertex.h"
#include "Triangle.h"

enum class RenderMode
{
	Triangle,
	Line,
	Wireframe
};

class MeshModel : public ModelBase
{
public:
	struct Light
	{
		alignas(16) glm::vec4 _direction; // Use vec4 instead of vec3 so that it can match the shader alignment
		alignas(16) glm::vec4 _color; // Ditto
		alignas(4) float _intensity;
	};

	struct Material
	{
		alignas(16) glm::vec4 _color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		alignas(16) glm::vec4 _specularColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);
		alignas(4) float _glossiness = 10.0f;
	};

private:
	std::vector<std::shared_ptr<MeshObject>> _meshObjects;

	// ==================== Render mode ====================
	RenderMode _renderMode = RenderMode::Triangle;
	VkPolygonMode _polygonMode = VK_POLYGON_MODE_FILL;
	VkPrimitiveTopology _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	float _lineWidth = 1.0f;

	// ==================== Pipeline and shaders ====================
	VkShaderModule _vertShaderModule;
	VkShaderModule _fragShaderModule;

	VkPipeline _graphicsPipeline;
	VkPipelineLayout _pipelineLayout;

	// ==================== Vertex input ====================
	std::vector<Vertex> _vertices;
	Buffer _vertexBuffer;

	// ==================== Index input ====================
	std::vector<uint32_t> _indices;
	Buffer _indexBuffer;

	// ==================== Triangles ====================
	std::shared_ptr<std::vector<Triangle>> _triangles = std::make_shared<std::vector<Triangle>>(); // Triangles in the model space

	// ==================== Texture ====================
	Image _texture;
	uint32_t _textureMipLevels = 0;

	VkSampler _textureSampler = VK_NULL_HANDLE; // Textures are accessed through samplers so that filters and transformations are applied to get rid of artifacts.

	// ==================== Descriptor ====================
	std::unique_ptr<DescriptorHelper> _descriptorHelper = nullptr;

	static const uint32_t MAX_SET_COUNT = 1000;
	VkDescriptorPool _descriptorPool;
	VkDescriptorSetLayout _descriptorSetLayout;
	std::vector<std::vector<VkDescriptorSet>> _descriptorSetsList; // [Mesh object count][Frames in flight]

	// ==================== Light ====================
	std::vector<Buffer> _lightBuffers;

	// ==================== Material ====================
	Material _material;
	std::vector<Buffer> _materialBuffers;

public:
	MeshModel();
	virtual ~MeshModel();

	virtual void Register() override;

	virtual void RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame) override;
	virtual uint32_t GetOrder() override { return 1000; }
	
	void LoadMesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);	
	void SetMeshBuffers(Buffer vertexBuffer, Buffer indexBuffer);
	void LoadPipeline(const std::wstring &vertexShaderPath, const std::wstring &fragmentShaderPath, RenderMode renderMode = RenderMode::Triangle);
	void LoadTexture(const std::wstring &texturePath);

	void UpdateVertices(const std::vector<Vertex> &vertices);
	void UpdateIndices(const std::vector<uint32_t> &indices);

	void SetMaterial(const Material &material);
	void SetMaterial(Material &&material);
	void SetLineWidth(float lineWidth) { _lineWidth = lineWidth; }

	std::shared_ptr<MeshObject> AddMeshObject();
	void RemoveMeshObject(const std::shared_ptr<MeshObject> &object);

private:
	void UpdateTriangles(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);

	std::tuple<VkDescriptorPool, VkDescriptorSetLayout> PrepareDescriptors();
	VkSampler CreateTextureSampler(uint32_t textureMipLevels);

	std::tuple<VkPipeline, VkPipelineLayout> CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule);

	void ApplyLightAdjustment(glm::vec3 direction, glm::vec3 color, float intensity);
	void ApplyMaterialAdjustment();

	std::tuple<Image, uint32_t> CreateTextureImage(const std::wstring &texturePath);
};