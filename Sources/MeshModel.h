#pragma once

#include "ModelBase.h"
#include "MeshObject.h"
#include "Vertex.h"

class MeshModel : public ModelBase
{
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

public:
	explicit MeshModel(const std::shared_ptr<VulkanCore> &vulkanCore);

	virtual void RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
	
	void LoadAssets(const std::string &OBJPath, const std::string &texturePath, const std::string &vertexShaderPath, const std::string &fragmentShaderPath);

	std::shared_ptr<MeshObject> AddMeshObject();
	void RemoveMeshObject(const std::shared_ptr<MeshObject> &object);

private:
	VkDescriptorSetLayout CreateDescriptorSetLayout();
	VkDescriptorPool CreateDescriptorPool(uint32_t maxSetCount);
	std::vector<VkDescriptorSet> CreateDescriptorSets(const std::vector<VkBuffer> &uniformBuffers);

	std::tuple<VkImage, VkDeviceMemory, VkImageView, uint32_t> CreateTextureImage(const std::string &texturePath);
	VkSampler CreateTextureSampler(uint32_t textureMipLevels);

	std::tuple<VkBuffer, VkDeviceMemory> CreateVertexBuffer(const std::vector<Vertex> &vertices);
	std::tuple<VkBuffer, VkDeviceMemory> CreateIndexBuffer(const std::vector<uint32_t> &indices);

	std::tuple<VkPipeline, VkPipelineLayout> CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule);

	void OnCleanUpOthers();
	void OnTransformCamera(const glm::mat4 &model, const glm::mat4 &project);

	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	std::tuple<std::vector<Vertex>, std::vector<uint32_t>> LoadOBJ(const std::string &OBJPath);
};