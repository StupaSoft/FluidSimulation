#pragma once

#include "ModelBase.h"
#include "MeshObject.h"
#include "Vertex.h"
#include "Triangle.h"

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
	std::vector<std::vector<VkDescriptorSet>> _descriptorSets; // [Mesh object count][Frames in flight]

	// ==================== Pipeline and shaders ====================
	VkShaderModule _vertShaderModule;
	VkShaderModule _fragShaderModule;

	VkPipeline _graphicsPipeline;
	VkPipelineLayout _pipelineLayout;

	// ==================== Vertex input ====================
	std::vector<Vertex> _vertices;
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;

	void *_vertexOnHost;
	VkBuffer _vertexStagingBuffer;
	VkDeviceMemory _vertexStagingBufferMemory;

	// ==================== Index input ====================
	std::vector<uint32_t> _indices;
	VkBuffer _indexBuffer;
	VkDeviceMemory _indexBufferMemory;

	void *_indexOnHost;
	VkBuffer _indexStagingBuffer;
	VkDeviceMemory _indexStagingBufferMemory;

	// ==================== Triangles ====================
	std::shared_ptr<std::vector<Triangle>> _triangles = std::make_shared<std::vector<Triangle>>(); // Triangles in the model space

	// ==================== Texture ====================
	VkImage _textureImage;
	VkDeviceMemory _textureImageMemory;
	VkImageView _textureImageView;
	uint32_t _textureMipLevels = 0;

	VkSampler _textureSampler; // Textures are accessed through samplers so that filters and transformations are applied to get rid of artifacts.

	// ==================== Descriptor ====================
	static const uint32_t MAX_SET_COUNT = 1000;
	VkDescriptorSetLayout _descriptorSetLayout;
	VkDescriptorPool _descriptorPool;

	// ==================== Light ====================
	std::vector<VkBuffer> _lightBuffers;
	std::vector<VkDeviceMemory> _lightBuffersMemory;

	// ==================== Material ====================
	Material _material;
	std::vector<VkBuffer> _materialBuffers;
	std::vector<VkDeviceMemory> _materialBuffersMemory;

public:
	explicit MeshModel(const std::shared_ptr<VulkanCore> &vulkanCore);

	virtual void RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
	virtual uint32_t GetOrder() override { return 1000; }
	
	void LoadMesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);	
	void LoadMesh(VkBuffer vertexBuffer, VkDeviceMemory vertexBufferMemory, VkBuffer indexBuffer, VkDeviceMemory indexBufferMemory);
	void LoadShaders(const std::string &vertexShaderPath, const std::string &fragmentShaderPath);
	void LoadTexture(const std::string &texturePath);

	void UpdateVertices(const std::vector<Vertex> &vertices);
	void UpdateIndices(const std::vector<uint32_t> &indices);

	void SetMaterial(const Material &material);
	void SetMaterial(Material &&material);

	std::shared_ptr<MeshObject> AddMeshObject();
	void RemoveMeshObject(const std::shared_ptr<MeshObject> &object);

private:
	void UpdateTriangles(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);

	VkDescriptorSetLayout CreateDescriptorSetLayout();
	VkDescriptorPool CreateDescriptorPool(uint32_t maxSetCount);
	std::vector<VkDescriptorSet> CreateDescriptorSets(const std::vector<VkBuffer> &mvpBuffers);

	VkSampler CreateTextureSampler(uint32_t textureMipLevels);

	std::tuple<VkPipeline, VkPipelineLayout> CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule);

	void OnCleanUpOthers();

	void ApplyLightAdjustment(glm::vec3 direction, glm::vec3 color, float intensity);
	void ApplyMaterialAdjustment();
};