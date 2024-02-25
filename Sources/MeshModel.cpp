#include "MeshModel.h"
#include "VulkanCore.h"

MeshModel::MeshModel(const std::shared_ptr<VulkanCore> &vulkanCore) : 
	ModelBase(vulkanCore),
	_descriptorHelper(vulkanCore)
{
	// Create resources
	_lightBuffers = CreateBuffers
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(Light),
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	_materialBuffers = CreateBuffers
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		sizeof(Material),
		_vulkanCore->GetMaxFramesInFlight(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	std::tie(_descriptorPool, _descriptorSetLayout) = PrepareDescriptors();

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

	// Load a fallback texture
	LoadTexture("");
}

void MeshModel::RecordCommand(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
	VkBuffer vertexBuffers[] = { _vertexBuffer._buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // Bind vertex buffers to bindings
	vkCmdBindIndexBuffer(commandBuffer, _indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32); // Bind index buffers to bindings

	size_t meshObjectCount = _meshObjects.size();
	for (size_t i = 0; i < meshObjectCount; ++i)
	{
		if (_meshObjects[i]->IsVisible())
		{
			auto &descriptorSets = _descriptorSetsList[i];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, _indexCount, 1, 0, 0, 0);
		}
	}
}

void MeshModel::OnCleanUpOthers()
{
	for (auto &meshObject : _meshObjects)
	{
		meshObject->CleanUp();
	}

	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), _vertexStagingBuffer._memory);
	DestroyBuffer(_vulkanCore->GetLogicalDevice(), _vertexStagingBuffer);

	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), _indexStagingBuffer._memory);
	DestroyBuffer(_vulkanCore->GetLogicalDevice(), _indexStagingBuffer);

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

	DestroyBuffer(_vulkanCore->GetLogicalDevice(), _vertexBuffer);
	DestroyBuffer(_vulkanCore->GetLogicalDevice(), _indexBuffer);

	DestroyBuffers(_vulkanCore->GetLogicalDevice(), _materialBuffers);
	DestroyBuffers(_vulkanCore->GetLogicalDevice(), _lightBuffers);
}

void MeshModel::UpdateTriangles(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
	const size_t STRIDE = 3;

	size_t triangleCount = indices.size() / STRIDE;
	_triangles->resize(triangleCount);

	#pragma omp parallel for
	for (size_t i = 0; i < triangleCount; ++i)
	{
		auto &triangle = _triangles->at(i);

		triangle.A = vertices[indices[i * STRIDE + 0]].pos;
		triangle.normalA = vertices[indices[i * STRIDE + 0]].normal;

		triangle.B = vertices[indices[i * STRIDE + 1]].pos;
		triangle.normalB = vertices[indices[i * STRIDE + 1]].normal;

		triangle.C = vertices[indices[i * STRIDE + 2]].pos;
		triangle.normalC = vertices[indices[i * STRIDE + 2]].normal;
	}
}

void MeshModel::LoadTexture(const std::string &texturePath)
{
	// Load a texture
	std::string targetTexturePath = texturePath;
	if (targetTexturePath.empty()) targetTexturePath = "Textures/Fallback.png"; // Fallback texture
	std::tie(_textureImage, _textureImageMemory, _textureImageView, _textureMipLevels) = CreateTextureImage(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), targetTexturePath);
	_textureSampler = CreateTextureSampler(_textureMipLevels);
}

void MeshModel::LoadMesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
	// Create a vertex (creation, update)
	uint32_t vertexBufferSize = static_cast<uint32_t>(sizeof(vertices[0]) * vertices.size());
	_vertexBuffer = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_vertexStagingBuffer = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Allocate separate buffer, one on the CPU and the other on the GPU.
	// Temporary, host-visible buffer that resides on the CPU
	_vertexStagingBuffer = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(_vulkanCore->GetLogicalDevice(), _vertexStagingBuffer._memory, 0, vertexBufferSize, 0, &_vertexOnHost);
	UpdateVertices(vertices);

	// Create an index buffer
	uint32_t indexBufferSize = static_cast<uint32_t>(sizeof(indices[0]) * indices.size());
	_indexBuffer = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_indexStagingBuffer = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	_indexStagingBuffer= CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(_vulkanCore->GetLogicalDevice(), _indexStagingBuffer._memory, 0, indexBufferSize, 0, &_indexOnHost);
	UpdateIndices(indices);
}

void MeshModel::SetMeshBuffers(Buffer vertexBuffer, Buffer indexBuffer, uint32_t indexCount)
{
	_vertexBuffer = vertexBuffer;
	_indexBuffer = indexBuffer;
	_indexCount = indexCount;
}

void MeshModel::LoadShaders(const std::string &vertexShaderPath, const std::string &fragmentShaderPath)
{
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
	std::shared_ptr<MeshObject> meshObject = std::make_shared<MeshObject>(_vulkanCore, _triangles);
	_meshObjects.push_back(meshObject);

	const auto &mvpBuffers = meshObject->GetMVPBuffers();

	_descriptorHelper.BindBuffers(0, mvpBuffers);
	_descriptorHelper.BindBuffers(1, _lightBuffers);
	_descriptorHelper.BindBuffers(2, _materialBuffers);
	_descriptorHelper.BindSampler(3, _textureSampler, _textureImageView);

	_descriptorSetsList.emplace_back(_descriptorHelper.GetDescriptorSets());

	return meshObject;
}

void MeshModel::RemoveMeshObject(const std::shared_ptr<MeshObject> &object)
{
	auto it = std::find_if(_meshObjects.begin(), _meshObjects.end(), [object](const auto &meshObject) { return meshObject == object; });
	std::iter_swap(it, _meshObjects.end() - 1);
	_meshObjects.pop_back();
}

// Create a descriptor pool from which we can allocate descriptor sets
std::tuple<VkDescriptorPool, VkDescriptorSetLayout> MeshModel::PrepareDescriptors()
{
	// Create descriptor pool
	_descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_SET_COUNT });
	_descriptorHelper.AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SET_COUNT });
	auto descriptorPool = _descriptorHelper.GetDescriptorPool();

	// Create descriptor set layout
	BufferLayout mvpLayout
	{
		.binding = 0,
		.dataSize = sizeof(MeshObject::MVP),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_VERTEX_BIT
	};

	BufferLayout lightLayout
	{
		.binding = 1,
		.dataSize = sizeof(Light),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_VERTEX_BIT
	};

	BufferLayout materialLayout
	{
		.binding = 2,
		.dataSize = sizeof(Material),
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.shaderStages = VK_SHADER_STAGE_VERTEX_BIT
	};

	SamplerLayout samplerLayout
	{
		.binding = 3
	};

	_descriptorHelper.AddBufferLayout(mvpLayout);
	_descriptorHelper.AddBufferLayout(lightLayout);
	_descriptorHelper.AddBufferLayout(materialLayout);
	_descriptorHelper.AddSamplerLayout(samplerLayout);

	auto descriptorSetLayout = _descriptorHelper.GetDescriptorSetLayout();

	return std::make_tuple(descriptorPool, descriptorSetLayout);
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

void MeshModel::UpdateVertices(const std::vector<Vertex> &vertices)
{
	_vertices = vertices;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	memcpy(_vertexOnHost, vertices.data(), (size_t)bufferSize); // Copy index data to the mapped memory
	CopyBufferToBuffer(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), _vertexStagingBuffer, _vertexBuffer, bufferSize);

	UpdateTriangles(_vertices, _indices);
}

void MeshModel::UpdateIndices(const std::vector<uint32_t> &indices)
{
	_indices = indices;
	_indexCount = static_cast<uint32_t>(_indices.size());

	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	memcpy(_indexOnHost, indices.data(), (size_t)bufferSize); // Copy index data to the mapped memory
	CopyBufferToBuffer(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), _indexStagingBuffer, _indexBuffer, bufferSize);

	UpdateTriangles(_vertices, _indices);
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
	CopyMemoryToBuffers(_vulkanCore->GetLogicalDevice(), _lightBuffers, &light, copyOffset, copySize);
}

void MeshModel::ApplyMaterialAdjustment()
{
	auto copyOffset = 0;
	auto copySize = sizeof(Material);
	CopyMemoryToBuffers(_vulkanCore->GetLogicalDevice(), _materialBuffers, &_material, copyOffset, copySize);
}
