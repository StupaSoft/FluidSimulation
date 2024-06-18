#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

#include "MeshModel.h"
#include "VulkanCore.h"

MeshModel::MeshModel()
{
	_descriptorHelper = std::make_unique<DescriptorHelper>();

	// Create resources
	_lightBuffers = CreateBuffers(sizeof(Light), VulkanCore::Get()->GetMaxFramesInFlight(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_materialBuffers = CreateBuffers(sizeof(Material), VulkanCore::Get()->GetMaxFramesInFlight(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	std::tie(_descriptorPool, _descriptorSetLayout) = PrepareDescriptors();

	ApplyMaterialAdjustment();

	// Load the fallback texture
	LoadTexture(L"");
}

MeshModel::~MeshModel()
{
	vkDeviceWaitIdle(VulkanCore::Get()->GetLogicalDevice());

	for (auto &meshObject : _meshObjects)
	{
		meshObject->CleanUp();
	}

	vkDestroyPipeline(VulkanCore::Get()->GetLogicalDevice(), _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::Get()->GetLogicalDevice(), _pipelineLayout, nullptr);

	vkDestroySampler(VulkanCore::Get()->GetLogicalDevice(), _textureSampler, nullptr);

	vkDestroyDescriptorPool(VulkanCore::Get()->GetLogicalDevice(), _descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::Get()->GetLogicalDevice(), _descriptorSetLayout, nullptr);
}

void MeshModel::Register()
{
	ModelBase::Register();

	auto &mainLight = VulkanCore::Get()->GetMainLight();
	ApplyLightAdjustment(mainLight->GetDirection(), mainLight->GetColor(), mainLight->GetIntensity());
	mainLight->OnChanged().AddListener
	(
		weak_from_this(),
		[this](const DirectionalLight &light)
		{
			ApplyLightAdjustment(light.GetDirection(), light.GetColor(), light.GetIntensity());
		}
	);
}

void MeshModel::RecordCommand(VkCommandBuffer commandBuffer, size_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
	VkBuffer vertexBuffers[] = { _vertexBuffer->GetBufferHandle()};
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // Bind vertex buffers to bindings
	vkCmdBindIndexBuffer(commandBuffer, _indexBuffer->GetBufferHandle(), 0, VK_INDEX_TYPE_UINT32); // Bind index buffers to bindings

	if (_renderMode == RenderMode::Line) vkCmdSetLineWidth(commandBuffer, _lineWidth);

	size_t meshObjectCount = _meshObjects.size();
	for (size_t i = 0; i < meshObjectCount; ++i)
	{
		if (_meshObjects[i]->IsVisible())
		{
			auto &descriptorSets = _descriptorSetsList[i];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, _indexBuffer->Size() / sizeof(uint32_t), 1, 0, 0, 0);
		}
	}
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

		triangle.A = glm::vec4(vertices[indices[i * STRIDE + 0]].pos, 0.0f);
		triangle.normalA = glm::vec4(vertices[indices[i * STRIDE + 0]].normal, 1.0f);

		triangle.B = glm::vec4(vertices[indices[i * STRIDE + 1]].pos, 0.0f);
		triangle.normalB = glm::vec4(vertices[indices[i * STRIDE + 1]].normal, 1.0f);

		triangle.C = glm::vec4(vertices[indices[i * STRIDE + 2]].pos, 0.0f);
		triangle.normalC = glm::vec4(vertices[indices[i * STRIDE + 2]].normal, 1.0f);
	}
}

void MeshModel::LoadTexture(const std::wstring &texturePath)
{
	// We have to free prior images
	if (_textureSampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(VulkanCore::Get()->GetLogicalDevice(), _textureSampler, nullptr);
	}

	// Load a texture
	std::wstring targetTexturePath = texturePath;
	if (targetTexturePath.empty()) targetTexturePath = L"Textures/Fallback.png"; // Fallback texture
	std::tie(_texture, _textureMipLevels) = CreateTextureImage(targetTexturePath);
	_textureSampler = CreateTextureSampler(_textureMipLevels);
}

void MeshModel::LoadMesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Create a vertex
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	_vertexBuffer = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	
	// Create an index buffer
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
	_indexBuffer = CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	memory->Bind({ _vertexBuffer, _indexBuffer });

	// Update once
	UpdateVertices(vertices);
	UpdateIndices(indices);
}

void MeshModel::SetMeshBuffers(Buffer vertexBuffer, Buffer indexBuffer)
{
	_vertexBuffer = vertexBuffer;
	_indexBuffer = indexBuffer;
}

void MeshModel::LoadPipeline(const std::wstring &vertexShaderPath, const std::wstring &fragmentShaderPath, RenderMode renderMode)
{
	_renderMode = renderMode;
	if (renderMode == RenderMode::Triangle)
	{
		_polygonMode = VK_POLYGON_MODE_FILL;
		_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
	else if (renderMode == RenderMode::Wireframe)
	{
		_polygonMode = VK_POLYGON_MODE_LINE;
		_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
	else if (renderMode == RenderMode::Line)
	{
		_polygonMode = VK_POLYGON_MODE_LINE;
		_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	}

	// Create shader modules
	_vertShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(vertexShaderPath));
	_fragShaderModule = CreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), ReadFile(fragmentShaderPath));

	// Create a graphics pipeline
	std::tie(_graphicsPipeline, _pipelineLayout) = CreateGraphicsPipeline(_descriptorSetLayout, _vertShaderModule, _fragShaderModule);

	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), _fragShaderModule, nullptr);
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), _vertShaderModule, nullptr);
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
	std::shared_ptr<MeshObject> meshObject = MeshObject::Instantiate<MeshObject>(_triangles);
	_meshObjects.push_back(meshObject);

	const auto &mvpBuffers = meshObject->GetMVPBuffers();

	_descriptorHelper->BindBuffers(0, mvpBuffers);
	_descriptorHelper->BindBuffers(1, _lightBuffers);
	_descriptorHelper->BindBuffers(2, _materialBuffers);
	_descriptorHelper->BindSampler(3, _textureSampler, _texture);

	_descriptorSetsList.emplace_back(_descriptorHelper->GetDescriptorSets());

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
	_descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_SET_COUNT });
	_descriptorHelper->AddDescriptorPoolSize({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SET_COUNT });
	auto descriptorPool = _descriptorHelper->GetDescriptorPool();

	_descriptorHelper->AddBufferLayout(0, sizeof(MeshObject::MVP), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	_descriptorHelper->AddBufferLayout(1, sizeof(Light), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	_descriptorHelper->AddBufferLayout(2, sizeof(Material), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	_descriptorHelper->AddSamplerLayout(3);
	auto descriptorSetLayout = _descriptorHelper->GetDescriptorSetLayout();

	return std::make_tuple(descriptorPool, descriptorSetLayout);
}

VkSampler MeshModel::CreateTextureSampler(uint32_t textureMipLevels)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(VulkanCore::Get()->GetPhysicalDevice(), &properties);

	// Specify filters and transformations to apply
	VkSamplerCreateInfo samplerInfo
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
	if (vkCreateSampler(VulkanCore::Get()->GetLogicalDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture sampler.");
	}

	return textureSampler;
}

void MeshModel::UpdateVertices(const std::vector<Vertex> &vertices)
{
	_vertices = vertices;
	_vertexBuffer->CopyFrom(vertices.data());
	UpdateTriangles(vertices, _indices);
}

void MeshModel::UpdateIndices(const std::vector<uint32_t> &indices)
{
	_indices = indices;
	_indexBuffer->CopyFrom(indices.data());
	UpdateTriangles(_vertices, indices);
}

std::tuple<VkPipeline, VkPipelineLayout> MeshModel::CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule)
{
	// Assign shaders to a specific pipeline stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT, // Shader stage
		.module = vertShaderModule,
		.pName = "main" // Entry point
	};

	VkPipelineShaderStageCreateInfo fragShaderStageInfo
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
	VkPipelineVertexInputStateCreateInfo vertexInputInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
		.pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
	};

	// Configure an input assembly
	// Describles what kind of geometry will be drawn from the vertices and if primitive restart should be enabled
	VkPipelineInputAssemblyStateCreateInfo inputAssembly
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = _topology, // Triangle from every three vertices without reuse
		.primitiveRestartEnable = VK_FALSE
	};

	// 'Resize' the image
	VkViewport viewport
	{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)VulkanCore::Get()->GetExtent().width, // Can differ from the extent of the window
		.height = (float)VulkanCore::Get()->GetExtent().height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	// 'Crop' the image
	VkRect2D scissor
	{
		.offset = {0, 0},
		.extent = VulkanCore::Get()->GetExtent()
	};

	VkPipelineViewportStateCreateInfo viewportState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	// Configure a rasterizer that turns geometry formed by the vertex shader into fragments to be colored by the fragment shader
	VkPipelineRasterizationStateCreateInfo rasterizer
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // Whether to clamp planes out of range to near and far clipping planes
		.rasterizerDiscardEnable = VK_FALSE, // When set to true, geometry never passes through the rasterization stage
		.polygonMode = _polygonMode,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // Since we flipped the projection
		.depthBiasEnable = VK_FALSE,
		.lineWidth = _lineWidth
	};

	// Configure multisampling
	VkPipelineMultisampleStateCreateInfo multisampling
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = GetMaxUsableSampleCount(VulkanCore::Get()->GetPhysicalDevice()),
		.sampleShadingEnable = VK_TRUE, // Enable sample shading in the pipeline
		.minSampleShading = 1.0f, // Minimum fraction for sample shading; closer to one is smoother
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	// After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer.
	// Per-framebuffer configuration
	VkPipelineColorBlendAttachmentState colorBlendAttachment
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
	VkPipelineColorBlendStateCreateInfo colorBlending
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE, // Whether to use logic operation to blend colors, instead of blendEnable
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
	};

	// Enable depth test
	VkPipelineDepthStencilStateCreateInfo depthStencil
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
	VkPipelineDynamicStateCreateInfo dynamicState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// Pipeline layout
	// Specify uniform values
	VkPipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &_descriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr, // A way of passing dynamic values to shaders
	};

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(VulkanCore::Get()->GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a pipeline layout.");
	}

	// Finally create a complete graphics pipeline with the following components
	// 1. Shader stages
	// 2. Fixed-function state
	// 3. Pipeline layout
	// 4. Render pass
	VkGraphicsPipelineCreateInfo pipelineInfo
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
		.renderPass = VulkanCore::Get()->GetRenderPass(),
		.subpass = 0,

		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	if (vkCreateGraphicsPipelines(VulkanCore::Get()->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
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

	for (auto &lightBuffer : _lightBuffers)
	{
		lightBuffer->CopyFrom(&light);
	}
}

void MeshModel::ApplyMaterialAdjustment()
{
	for (auto &materialBuffer : _materialBuffers)
	{
		materialBuffer->CopyFrom(&_material);
	}
}

std::tuple<Image, uint32_t> MeshModel::CreateTextureImage(const std::wstring &texturePath)
{
	// Load a texture image
	int width = 0;
	int height = 0;
	int texChannels = 0;

	std::string pathInUTF8;
	stbi_convert_wchar_to_utf8(pathInUTF8.data(), texturePath.length() + 1, texturePath.c_str());
	stbi_uc *pixels = stbi_load(pathInUTF8.data(), &width, &height, &texChannels, STBI_rgb_alpha);
	if (!pixels)
	{
		std::wcout << std::format(L"Failed to open the texture file: {0}", texturePath) << std::endl;
		throw std::runtime_error("");
	}

	VkDeviceSize imageSize = width * height * 4; // Four bytes per pixel
	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1; // How many times the image can be divided by 2?

	// Create an image object
	// Specify VK_IMAGE_USAGE_TRANSFER_SRC_BIT so that it can be used as a blit source
	Image texture = CreateImage
	(
		width,
		height,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT - The image is going to be used as a destination for the buffer copy from the staging buffer
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT - We will create mipmaps by transfering the image
		// VK_IMAGE_USAGE_SAMPLED_BIT - The texture will be sampled later
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	// Before we use vkCmdCopyBufferToImage, the image must be in a proper layout.
	// Hence, transfer the image layout from undefined to transfer optimal

	// Image memory barriers are generally used to synchronize access to resources, but it can also be used to transition
	// image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.
	VkImageMemoryBarrier textureBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

		// We want to transfer the image layout from oldLayout to newLayout.
		.srcAccessMask = 0, // Writes don't have to wait on anything
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

		// We don't want to transfer queue family ownership
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

		.image = texture->GetImageHandle(),
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Earliest possible pipeline stage
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // Pseudo-stage where transfers happen

	VkCommandBuffer texCommandBuffer = VulkanCore::Get()->BeginSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool());
	vkCmdPipelineBarrier(texCommandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &textureBarrier); // Transfer the image layout
	VulkanCore::Get()->EndSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool(), texCommandBuffer, VulkanCore::Get()->GetGraphicsQueue());

	// Now copy the buffer to the image
	texture->CopyFrom(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	stbi_image_free(pixels); // Clean up the original pixel array

	// Generate mipmaps
	// Will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	// Check if the image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(VulkanCore::Get()->GetPhysicalDevice(), VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("The texture image format does not support linear blitting.");
	}

	// repeatedly blit a source image to a smaller one
	VkCommandBuffer mipCommandBuffer = VulkanCore::Get()->BeginSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool());
	VkImageMemoryBarrier mipBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = texture->GetImageHandle(),
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	int32_t mipWidth = static_cast<int32_t>(width);
	int32_t mipHeight = static_cast<int32_t>(height);
	// (i - 1)'th layout
	// 1. VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	// 2. Blit
	// 3. VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	for (uint32_t i = 1; i < mipLevels; ++i)
	{
		mipBarrier.subresourceRange.baseMipLevel = i - 1; // Process (i - 1)'th mip level
		mipBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Transition level (i - 1) to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		mipBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(mipCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mipBarrier); // Wait for level i - 1 to be filled with previous blit command or vkCmdCopyBufferToImage

		// Blit area
		VkImageBlit blit
		{
			.srcSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i - 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.srcOffsets = { { 0, 0, 0 }, { mipWidth, mipHeight, 1 } },
			.dstSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.dstOffsets = { { 0, 0, 0 }, { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 } }
		};

		vkCmdBlitImage(mipCommandBuffer, texture->GetImageHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->GetImageHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR); // Record the blit command - both the source and the destination images are same, because they are blitting between different mip levels.

		mipBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mipBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// This transition to the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL flag waits on the current blit command to finish.
		vkCmdPipelineBarrier(mipCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mipBarrier);

		// Divide the dimensions with a guarantee that they won't be 0
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition the last mipmap
	mipBarrier.subresourceRange.baseMipLevel = mipLevels - 1; // Process the last mipmap
	mipBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	mipBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mipBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	mipBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(mipCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mipBarrier);

	VulkanCore::Get()->EndSingleTimeCommands(VulkanCore::Get()->GetGraphicsCommandPool(), mipCommandBuffer, VulkanCore::Get()->GetGraphicsQueue());

	return std::make_tuple(texture, mipLevels);
}

