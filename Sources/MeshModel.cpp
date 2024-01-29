#include "MeshModel.h"
#include "VulkanCore.h"

const uint32_t MeshModel::MAX_SET_COUNT = 1000;

MeshModel::MeshModel(const std::shared_ptr<VulkanCore> &vulkanCore)
	: ModelBase(vulkanCore)
{
	// Create resources
	std::tie(_lightBuffers, _lightBuffersMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(Light),
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	std::tie(_materialBuffers, _materialBuffersMemory) = CreateBuffersAndMemory
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(Material),
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	_descriptorSetLayout = CreateDescriptorSetLayout();
	_descriptorPool = CreateDescriptorPool(MAX_SET_COUNT);

	// Initialize & add callbacks
	_vulkanCore->OnCleanUpOthers().AddListener
	(
		[this]()
		{
			OnCleanUpOthers();
		}
	);

	ApplyMaterialAdjustment();

	auto &mainLight = _vulkanCore->GetMainLight();
	ApplyLightAdjustment(mainLight->GetDirection(), mainLight->GetColor(), mainLight->GetIntensity());
	mainLight->OnChanged().AddListener
	(
		[this](const DirectionalLight &light)
		{
			ApplyLightAdjustment(light.GetDirection(), light.GetColor(), light.GetIntensity());
		}
	);
}

void MeshModel::RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
	VkBuffer vertexBuffers[] = { _vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // Bind vertex buffers to bindings
	vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32); // Bind index buffers to bindings
	for (auto &objectPair : _objectPairs)
	{
		auto descriptorSets = std::get<1>(objectPair);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);
	}
}

void MeshModel::OnCleanUpOthers()
{
	for (auto &objectPair : _objectPairs)
	{
		auto object = std::get<0>(objectPair);
		object->CleanUp();
	}

	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), _vertexStagingBufferMemory);
	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _vertexStagingBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), _vertexStagingBufferMemory, nullptr);

	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), _indexStagingBufferMemory);
	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _indexStagingBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), _indexStagingBufferMemory, nullptr);

	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), _fragShaderModule, nullptr);
	vkDestroyShaderModule(_vulkanCore->GetLogicalDevice(), _vertShaderModule, nullptr);

	vkDestroyPipeline(_vulkanCore->GetLogicalDevice(), _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(_vulkanCore->GetLogicalDevice(), _pipelineLayout, nullptr);
	
	vkDestroySampler(_vulkanCore->GetLogicalDevice(), _textureSampler, nullptr);
	vkDestroyImageView(_vulkanCore->GetLogicalDevice(), _textureImageView, nullptr);
	vkDestroyImage(_vulkanCore->GetLogicalDevice(), _textureImage, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), _textureImageMemory, nullptr);

	vkDestroyDescriptorPool(_vulkanCore->GetLogicalDevice(), _descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), _descriptorSetLayout, nullptr);

	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _vertexBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), _vertexBufferMemory, nullptr);

	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _indexBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), _indexBufferMemory, nullptr);

	for (size_t i = 0; i < _lightBuffers.size(); ++i)
	{
		vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _materialBuffers[i], nullptr);
		vkFreeMemory(_vulkanCore->GetLogicalDevice(), _materialBuffersMemory[i], nullptr);

		vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), _lightBuffers[i], nullptr);
		vkFreeMemory(_vulkanCore->GetLogicalDevice(), _lightBuffersMemory[i], nullptr);
	}
}

void MeshModel::LoadAssets(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, const std::string &vertexShaderPath, const std::string fragmentShaderPath, const std::string &texturePath)
{
	// Create a vertex (creation, update)
	uint32_t vertexBufferSize = static_cast<uint32_t>(sizeof(vertices[0]) * vertices.size());
	std::tie(_vertexBuffer, _vertexBufferMemory) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	std::tie(_vertexStagingBuffer, _vertexStagingBufferMemory) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Allocate separate buffer, one on the CPU and the other on the GPU.
	// Temporary, host-visible buffer that resides on the CPU
	std::tie(_vertexStagingBuffer, _vertexStagingBufferMemory) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(_vulkanCore->GetLogicalDevice(), _vertexStagingBufferMemory, 0, vertexBufferSize, 0, &_vertexOnHost);
	UpdateVertexBuffer(vertices);

	// Create an index buffer
	uint32_t indexBufferSize = static_cast < uint32_t>(sizeof(indices[0]) * indices.size());
	std::tie(_indexBuffer, _indexBufferMemory) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	std::tie(_indexStagingBuffer, _indexStagingBufferMemory) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	std::tie(_indexStagingBuffer, _indexStagingBufferMemory) = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(_vulkanCore->GetLogicalDevice(), _indexStagingBufferMemory, 0, indexBufferSize, 0, &_indexOnHost);
	UpdateIndexBuffer(indices);

	// Load a texture
	std::string targetTexturePath = texturePath;
	if (targetTexturePath.empty()) targetTexturePath = "Textures/Fallback.png"; // Fallback texture
	std::tie(_textureImage, _textureImageMemory, _textureImageView, _textureMipLevels) = CreateTextureImage(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), targetTexturePath);
	_textureSampler = CreateTextureSampler(_textureMipLevels);

	// Create shader modules
	_vertShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile(vertexShaderPath));
	_fragShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile(fragmentShaderPath));

	// Create a graphics pipeline
	std::tie(_graphicsPipeline, _pipelineLayout) = CreateGraphicsPipeline(_descriptorSetLayout, _vertShaderModule, _fragShaderModule);
}

void MeshModel::SetMaterial(const Material &material)
{
	_material = material;
	ApplyMaterialAdjustment();
}

void MeshModel::SetMaterial(Material &&material)
{
	_material = std::move(material);
	ApplyMaterialAdjustment();
}

std::shared_ptr<MeshObject> MeshModel::AddMeshObject()
{
	std::shared_ptr<MeshObject> meshObject = std::make_shared<MeshObject>(_vulkanCore);
	std::vector<VkDescriptorSet> descriptorSets = CreateDescriptorSets(meshObject->GetMVPBuffers());
	_objectPairs.push_back(std::make_tuple(meshObject, descriptorSets));

	return meshObject;
}

void MeshModel::RemoveMeshObject(const std::shared_ptr<MeshObject> &object)
{
	auto it = std::find_if(_objectPairs.begin(), _objectPairs.end(), [object](const auto &pair) { return std::get<0>(pair) == object; });
	std::iter_swap(it, _objectPairs.end() - 1);
	_objectPairs.pop_back();
}

VkDescriptorSetLayout MeshModel::CreateDescriptorSetLayout()
{
	// Descriptor layout specifies the types of resources that are going to be accessed by the pipeline, 
	// just like a render pass specifies the types of attachments that will be accessed.

	// Buffer object descriptor
	VkDescriptorSetLayoutBinding mvpLayoutBinding =
	{
		.binding = 0, // 'binding' in the shader - layout(binding = 0) uniform MVPMatrix
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1, // The number of values in the array of uniform buffer objects - only uniform buffer
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // In which shader stages the descriptor is going to be referenced
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutBinding lightLayoutBinding =
	{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutBinding materialLayoutBinding =
	{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr
	};

	// Combined texture sampler descriptor
	VkDescriptorSetLayoutBinding samplerLayoutBinding =
	{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr
	};

	std::vector<VkDescriptorSetLayoutBinding> bindings = { mvpLayoutBinding, lightLayoutBinding, materialLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(_vulkanCore->GetLogicalDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor set layout.");
	}

	return descriptorSetLayout;
}

// Create a descriptor pool from which we can allocate descriptor sets
VkDescriptorPool MeshModel::CreateDescriptorPool(uint32_t maxSetCount)
{
	std::vector<VkDescriptorPoolSize> poolSizes = // Which descriptor types descriptor sets are going to contain and how many of them
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSetCount },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSetCount }
	};

	VkDescriptorPoolCreateInfo poolInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(maxSetCount),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(_vulkanCore->GetLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}

	return descriptorPool;
}

// Create a descriptor set for each VkBuffer resource to bind it to the uniform buffer descriptor
std::vector<VkDescriptorSet> MeshModel::CreateDescriptorSets(const std::vector<VkBuffer> &mvpBuffers)
{
	std::vector<VkDescriptorSetLayout> layouts(_vulkanCore->GetMaxFramesInFlight(), _descriptorSetLayout); // Same descriptor set layouts for all descriptor sets

	VkDescriptorSetAllocateInfo allocInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool, // Descriptor pool to allocate from
		.descriptorSetCount = _vulkanCore->GetMaxFramesInFlight(), // The number of descriptor sets to allocate
		.pSetLayouts = layouts.data()
	};

	std::vector<VkDescriptorSet> descriptorSets(_vulkanCore->GetMaxFramesInFlight());
	if (vkAllocateDescriptorSets(_vulkanCore->GetLogicalDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// The configuration of descriptors is updated using the vkUpdateDescriptorSets function, 
	// which takes an array of VkWriteDescriptorSet structs as parameter.
	for (size_t i = 0; i < _vulkanCore->GetMaxFramesInFlight(); ++i)
	{
		// Specify the buffer and the region within it that contains the data for the descriptor.
		VkDescriptorBufferInfo mvpInfo =
		{
			.buffer = mvpBuffers[i],
			.offset = 0,
			.range = sizeof(MeshObject::MVP)
		};

		VkDescriptorBufferInfo lightInfo =
		{
			.buffer = _lightBuffers[i],
			.offset = 0,
			.range = sizeof(Light)
		};

		VkDescriptorBufferInfo materialInfo =
		{
			.buffer = _materialBuffers[i],
			.offset = 0,
			.range = sizeof(Material)
		};

		VkDescriptorImageInfo imageInfo =
		{
			.sampler = _textureSampler,
			.imageView = _textureImageView, // Now a shader can access the image view
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		std::vector<VkWriteDescriptorSet> descriptorWrites =
		{
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i], // Descriptor set to update
				.dstBinding = 0, // Binding index of the uniform buffer; 0
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

				.pBufferInfo = &mvpInfo, // Array with descriptorCount structs that actually configure the descriptors
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i],
				.dstBinding = 1,
				.dstArrayElement = 0,

				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

				.pBufferInfo = &lightInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i],
				.dstBinding = 2,
				.dstArrayElement = 0,

				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

				.pBufferInfo = &materialInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i], // Descriptor set to update
				.dstBinding = 3, // Binding index of the texture sampler; 3
				.dstArrayElement = 0, // Array is not used, so it is set to just 0.

				.descriptorCount = 1, // The number of array elements to update 
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

				.pImageInfo = &imageInfo
			}
		};

		vkUpdateDescriptorSets(_vulkanCore->GetLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return descriptorSets;
}

VkSampler MeshModel::CreateTextureSampler(uint32_t textureMipLevels)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(_vulkanCore->GetPhysicalDevice(), &properties);

	// Specify filters and transformations to apply
	VkSamplerCreateInfo samplerInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR, // Deals with oversampling
		.minFilter = VK_FILTER_LINEAR, // Deals with undersampling

		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,

		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

		.mipLodBias = 0.0f,

		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,

		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,

		.minLod = 0.0f,
		.maxLod = static_cast<float>(textureMipLevels),

		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE, // Use [0, 1) range on all axes
	};

	VkSampler textureSampler = VK_NULL_HANDLE;
	if (vkCreateSampler(_vulkanCore->GetLogicalDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture sampler.");
	}

	return textureSampler;
}

void MeshModel::UpdateVertexBuffer(const std::vector<Vertex> &vertices)
{
	_vertices = vertices;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	memcpy(_vertexOnHost, vertices.data(), (size_t)bufferSize); // Copy index data to the mapped memory
	CopyBuffer(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), _vertexStagingBuffer, _vertexBuffer, bufferSize);
}

void MeshModel::UpdateIndexBuffer(const std::vector<uint32_t> &indices)
{
	_indices = indices;

	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	memcpy(_indexOnHost, indices.data(), (size_t)bufferSize); // Copy index data to the mapped memory
	CopyBuffer(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), _indexStagingBuffer, _indexBuffer, bufferSize);
}

std::tuple<VkPipeline, VkPipelineLayout> MeshModel::CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule)
{
	// Assign shaders to a specific pipeline stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT, // Shader stage
		.module = vertShaderModule,
		.pName = "main" // Entry point
	};

	VkPipelineShaderStageCreateInfo fragShaderStageInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT, // Shader stage
		.module = fragShaderModule,
		.pName = "main" // Entry point
	};

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto vertexBindingDescription = Vertex::GetBindingDescription();
	auto vertexAttributeDescriptions = Vertex::GetAttributeDescriptions();

	// Configure vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
		.pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
	};

	// Configure an input assembly
	// Describles what kind of geometry will be drawn from the vertices and if primitive restart should be enabled
	VkPipelineInputAssemblyStateCreateInfo inputAssembly =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // Triangle from every three vertices without reuse
		.primitiveRestartEnable = VK_FALSE
	};

	// 'Resize' the image
	VkViewport viewport =
	{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)_vulkanCore->GetExtent().width, // Can differ from the extent of the window
		.height = (float)_vulkanCore->GetExtent().height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	// 'Crop' the image
	VkRect2D scissor =
	{
		.offset = {0, 0},
		.extent = _vulkanCore->GetExtent()
	};

	VkPipelineViewportStateCreateInfo viewportState =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	// Configure a rasterizer that turns geometry formed by the vertex shader into fragments to be colored by the fragment shader
	VkPipelineRasterizationStateCreateInfo rasterizer =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // Whether to clamp planes out of range to near and far clipping planes
		.rasterizerDiscardEnable = VK_FALSE, // When set to true, geometry never passes through the rasterization stage
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // Since we flipped the projection
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	// Configure multisampling
	VkPipelineMultisampleStateCreateInfo multisampling =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = GetMaxUsableSampleCount(_vulkanCore->GetPhysicalDevice()),
		.sampleShadingEnable = VK_TRUE, // Enable sample shading in the pipeline
		.minSampleShading = 1.0f, // Minimum fraction for sample shading; closer to one is smoother
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	// After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer.
	// Per-framebuffer configuration
	VkPipelineColorBlendAttachmentState colorBlendAttachment =
	{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	// Global color blending settings 
	VkPipelineColorBlendStateCreateInfo colorBlending =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE, // Whether to use logic operation to blend colors, instead of blendEnable
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
	};

	// Enable depth test
	VkPipelineDepthStencilStateCreateInfo depthStencil =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE, // Specifies if the depth of new fragments should be compared to the depth buffer to see if they should be discarded
		.depthWriteEnable = VK_TRUE, // Specifies if the new depth of fragments that pass the depth test should actually be written to the depth buffer
		.depthCompareOp = VK_COMPARE_OP_LESS, // The depth of new fragments should be less to pass the test.
		.depthBoundsTestEnable = VK_FALSE, // Optional depth bound test - if enabled, keep only fragments that fall within the specified depth range
		.stencilTestEnable = VK_FALSE
	};

	// Dynamic state that makes it possible to modify values without recreating the pipeline
	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
	VkPipelineDynamicStateCreateInfo dynamicState =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// Pipeline layout
	// Specify uniform values
	VkPipelineLayoutCreateInfo pipelineLayoutInfo =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &_descriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr, // A way of passing dynamic values to shaders
	};

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(_vulkanCore->GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a pipeline layout.");
	}

	// Finally create a complete graphics pipeline with the following components
	// 1. Shader stages
	// 2. Fixed-function state
	// 3. Pipeline layout
	// 4. Render pass
	VkGraphicsPipelineCreateInfo pipelineInfo =
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,

		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,

		.layout = pipelineLayout,

		// Any graphics pipeline is bound to a specific subpass of a render pass
		.renderPass = _vulkanCore->GetRenderPass(),
		.subpass = 0,

		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	if (vkCreateGraphicsPipelines(_vulkanCore->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a graphics pipeline.");
	}

	return std::make_tuple(graphicsPipeline, pipelineLayout);
}

void MeshModel::ApplyLightAdjustment(glm::vec3 direction, glm::vec3 color, float intensity)
{
	Light light
	{
		._direction = glm::vec4(-direction, 0.0f), // Negate the direction because shaders usually require the direction toward the light
		._color = glm::vec4(color, 1.0f),
		._intensity = intensity
	};

	auto copyOffset = 0;
	auto copySize = sizeof(Light);
	CopyToBuffer(_vulkanCore->GetLogicalDevice(), _lightBuffersMemory, &light, copyOffset, copySize);
}

void MeshModel::ApplyMaterialAdjustment()
{
	auto copyOffset = 0;
	auto copySize = sizeof(Material);
	CopyToBuffer(_vulkanCore->GetLogicalDevice(), _materialBuffersMemory, &_material, copyOffset, copySize);
}
