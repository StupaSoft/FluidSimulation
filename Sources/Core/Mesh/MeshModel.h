#pragma once

#include "ModelBase.h"
#include "MeshObject.h"
#include "Descriptor.h"
#include "Vertex.h"
#include "Triangle.h"
#include "ShaderManager.h"
#include "Pipeline.h"

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
	GraphicsPipelineOptions _graphicsPipelineOptions
	{
		._topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		._polygonMode = VK_POLYGON_MODE_FILL,
		._lineWidth = 1.0f
	};

	// ==================== Pipeline and shaders ====================
	Shader _vertShader = nullptr;
	Shader _fragShader = nullptr;

	Pipeline _graphicsPipeline = nullptr;

	// ==================== Draw parameter ====================
	Buffer _drawArgumentBuffer = nullptr;

	// ==================== Vertex input ====================
	std::vector<Vertex> _vertices;
	Buffer _vertexBuffer = nullptr;

	// ==================== Index input ====================
	std::vector<uint32_t> _indices;
	Buffer _indexBuffer = nullptr;

	// ==================== Triangles ====================
	std::shared_ptr<std::vector<Triangle>> _triangles = std::make_shared<std::vector<Triangle>>(); // Triangles in the model space

	// ==================== Texture ====================
	Image _texture = nullptr;
	uint32_t _textureMipLevels = 0;

	VkSampler _textureSampler = VK_NULL_HANDLE; // Textures are accessed through samplers so that filters and transformations are applied to get rid of artifacts.

	// ==================== Descriptor ====================
	Descriptor _descriptor = nullptr;

	// ==================== Light ====================
	std::vector<Buffer> _lightBuffers;

	// ==================== Material ====================
	Material _material;
	std::vector<Buffer> _materialBuffers;

public:
	MeshModel();
	virtual ~MeshModel();

	virtual void Register() override;
	
	void LoadMesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);	
	void LoadMesh(Buffer vertexBuffer, Buffer indexBuffer, Buffer drawArgumentBuffer);
	void LoadPipeline(const std::string &vertexShaderStem, const std::string &fragmentShaderStem, const std::string &vertexShaderEntry = "main", const std::string &fragmentShaderEntry = "main", RenderMode renderMode = RenderMode::Triangle);
	void LoadTexture(const std::string &textureName);

	void UpdateVertices(const std::vector<Vertex> &vertices);
	void UpdateIndices(const std::vector<uint32_t> &indices);

	void SetMaterial(const Material &material);
	void SetMaterial(Material &&material);
	void SetLineWidth(float lineWidth) { _graphicsPipelineOptions._lineWidth = lineWidth; }

	std::shared_ptr<MeshObject> AddMeshObject();
	void RemoveMeshObject(const std::shared_ptr<MeshObject> &object);

protected:
	virtual void RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame) override;

private:
	void UpdateTriangles(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);

	VkSampler CreateTextureSampler(uint32_t textureMipLevels);

	void ApplyLightAdjustment(glm::vec3 direction, glm::vec3 color, float intensity);
	void ApplyMaterialAdjustment();

	std::tuple<Image, uint32_t> CreateTextureImage(const std::string &textureName);
};