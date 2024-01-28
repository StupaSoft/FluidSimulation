#pragma once

#include "ModelBase.h"
#include "MeshObject.h"
#include "Vertex.h"

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
	std::vector<std::tuple<std::shared_ptr<MeshObject>, std::vector<VkDescriptorSet>>> _objectPairs;

	// ==================== Pipeline and shaders ====================
	VkShaderModule _vertShaderModule;
	VkShaderModule _fragShaderModule;

	VkPipeline _graphicsPipeline;
	VkPipelineLayout _pipelineLayout;

	// ==================== Vertex input ====================
	std::vector<Vertex> _vertices;
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;

	// ==================== Index input ====================
	std::vector<uint32_t> _indices;
	VkBuffer _indexBuffer;
	VkDeviceMemory _indexBufferMemory;

	// ==================== Texture ====================
	VkImage _textureImage;
	VkDeviceMemory _textureImageMemory;
	VkImageView _textureImageView;
	uint32_t _textureMipLevels;

	VkSampler _textureSampler; // Textures are accessed through samplers so that filters and transformations are applied to get rid of artifacts.

	// ==================== Descriptor ====================
	static const uint32_t MAX_SET_COUNT;
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
	
	void LoadAssets(const std::string &OBJPath, const std::string &texturePath, const std::string &vertexShaderPath, const std::string &fragmentShaderPath);
	void SetMaterial(const Material &material);
	void SetMaterial(Material &&material);

	std::shared_ptr<MeshObject> AddMeshObject();
	void RemoveMeshObject(const std::shared_ptr<MeshObject> &object);

private:
	VkDescriptorSetLayout CreateDescriptorSetLayout();
	VkDescriptorPool CreateDescriptorPool(uint32_t maxSetCount);
	std::vector<VkDescriptorSet> CreateDescriptorSets(const std::vector<VkBuffer> &mvpBuffers);

	std::tuple<VkImage, VkDeviceMemory, VkImageView, uint32_t> CreateTextureImage(const std::string &texturePath);
	VkSampler CreateTextureSampler(uint32_t textureMipLevels);

	std::tuple<VkBuffer, VkDeviceMemory> CreateVertexBuffer(const std::vector<Vertex> &vertices);
	std::tuple<VkBuffer, VkDeviceMemory> CreateIndexBuffer(const std::vector<uint32_t> &indices);

	std::tuple<VkPipeline, VkPipelineLayout> CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule);

	void OnCleanUpOthers();

	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	void ApplyLightAdjustment(glm::vec3 direction, glm::vec3 color, float intensity);
	void ApplyMaterialAdjustment();

	std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::string &OBJPath);
};