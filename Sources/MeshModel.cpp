#include "MeshModel.h"
#include "VulkanCore.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

const uint32_t MeshModel::MAX_SET_COUNT = 1000;

MeshModel::MeshModel(const std::shared_ptr<VulkanCore> &vulkanCore)
	: ModelBase(vulkanCore)
{
	_vulkanCore->OnCleanUpOthers().AddListener
	(
		[this]()
		{
			OnCleanUpOthers();
		}
	);
	_vulkanCore->OnRecreateSwapChain().AddListener
	(
		[this]()
		{
			for (auto &objectPair : _objectPairs)
			{
				auto object = std::get<0>(objectPair);
				object->ApplyModelTransformation();
			}
		}
	);
	_vulkanCore->GetMainCamera()->OnChanged().AddListener
	(
		[this](const Camera &camera)
		{
			for (auto &objectPair : _objectPairs)
			{
				auto object = std::get<0>(objectPair);
				object->SetCameraTransformation(camera.GetViewMatrix(), camera.GetProjectionMatrix());
			}
		}
	);

	_descriptorSetLayout = CreateDescriptorSetLayout();
	_descriptorPool = CreateDescriptorPool(MAX_SET_COUNT);
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

	for (auto &objectPair : _objectPairs)
	{
		auto object = std::get<0>(objectPair);
		object->CleanUp();
	}
}

void MeshModel::LoadAssets(const std::string &OBJPath, const std::string &texturePath, const std::string &vertexShaderPath, const std::string &fragmentShaderPath)
{
	// Load OBJ
	std::tie(_vertices, _indices) = LoadOBJ(OBJPath);
	std::tie(_vertexBuffer, _vertexBufferMemory) = CreateVertexBuffer(_vertices);
	std::tie(_indexBuffer, _indexBufferMemory) = CreateIndexBuffer(_indices);

	// Load a texture
	std::tie(_textureImage, _textureImageMemory, _textureImageView, _textureMipLevels) = CreateTextureImage(texturePath);
	_textureSampler = CreateTextureSampler(_textureMipLevels);

	// Create shader modules
	_vertShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile(vertexShaderPath));
	_fragShaderModule = CreateShaderModule(_vulkanCore->GetLogicalDevice(), ReadFile(fragmentShaderPath));

	// Create a graphics pipeline
	std::tie(_graphicsPipeline, _pipelineLayout) = CreateGraphicsPipeline(_descriptorSetLayout, _vertShaderModule, _fragShaderModule);
}

std::shared_ptr<MeshObject> MeshModel::AddMeshObject()
{
	std::shared_ptr<MeshObject> meshObject = std::make_shared<MeshObject>(_vulkanCore);
	std::vector<VkDescriptorSet> descriptorSets = CreateDescriptorSets(meshObject->GetUniformBuffers());
	_objectPairs.push_back(std::make_tuple(meshObject, descriptorSets));

	// Set initial camera transformation
	auto &mainCamera = _vulkanCore->GetMainCamera();
	meshObject->SetCameraTransformation(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());

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
	VkDescriptorSetLayoutBinding uboLayoutBinding =
	{
		.binding = 0, // 'binding' in the shader - layout(binding = 0) uniform UniformBufferObject
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1, // The number of values in the array of uniform buffer objects - only uniform buffer
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // In which shader stages the descriptor is going to be referenced
		.pImmutableSamplers = nullptr
	};

	// Combined texture sampler descriptor
	VkDescriptorSetLayoutBinding samplerLayoutBinding =
	{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr
	};

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

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
std::vector<VkDescriptorSet> MeshModel::CreateDescriptorSets(const std::vector<VkBuffer> &uniformBuffers)
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
		VkDescriptorBufferInfo bufferInfo =
		{
			.buffer = uniformBuffers[i],
			.offset = 0,
			.range = sizeof(MVPMatrix)
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

				.pBufferInfo = &bufferInfo, // Array with descriptorCount structs that actually configure the descriptors
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[i], // Descriptor set to update
				.dstBinding = 1, // Binding index of the texture sampler; 1
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

std::tuple<VkImage, VkDeviceMemory, VkImageView, uint32_t> MeshModel::CreateTextureImage(const std::string &texturePath)
{
	// Load a texture image
	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;
	stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) throw std::runtime_error(std::format("Failed to load {0}", texturePath));

	VkDeviceSize imageSize = texWidth * texHeight * 4; // Four bytes per pixel

	uint32_t textureMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1; // How many times the image can be divided by 2?

	// The most optimal memory for the GPU has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU.
	// We hire two buffers; one staging buffer in CPU-accessible memory to upload the data and the final buffer in device-local memory.
	// Then, use a buffer copy command to move the data from the staging buffer to the actual vertex buffer.
	// Create a buffer in host-visible memory so that we can use vkMapMemory and copy the pixels to it.
	auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data = nullptr;
	vkMapMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data); // Map data <-> stagingBufferMemory
	memcpy(data, pixels, static_cast<size_t>(imageSize)); // Copy pixels -> data
	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory);

	// Clean up the original pixel array
	stbi_image_free(pixels);

	// Create an image object
	// Specify VK_IMAGE_USAGE_TRANSFER_SRC_BIT so that it can be used as a blit source
	auto [textureImage, textureImageMemory] = CreateImageAndMemory
	(
		_vulkanCore->GetPhysicalDevice(),
		_vulkanCore->GetLogicalDevice(),
		texWidth,
		texHeight,
		textureMipLevels, VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT - The image is going to be used as a destination for the buffer copy from the staging buffer
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT - We will create mipmaps by transfering the image
		// VK_IMAGE_USAGE_SAMPLED_BIT - The texture will be sampled later
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkImageView textureImageView = CreateImageView(_vulkanCore->GetLogicalDevice(), textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, textureMipLevels);

	// Before we use vkCmdCopyBufferToImage, the image must be in a proper layout.
	// Hence, transfer the image layout from undefined to transfer optimal

	// Image memory barriers are generally used to synchronize access to resources, but it can also be used to transition
	// image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.
	VkImageMemoryBarrier barrier =
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

		.image = textureImage,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = textureMipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Earliest possible pipeline stage
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // Pseudo-stage where transfers happen

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool());
	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	EndSingleTimeCommands(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), commandBuffer, _vulkanCore->GetGraphicsQueue());

	// Now copy the buffer to the image
	CopyBufferToImage(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue(), stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory, nullptr);

	// Generate mipmaps
	// Will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	GenerateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, textureMipLevels);

	return std::make_tuple(textureImage, textureImageMemory, textureImageView, textureMipLevels);
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

std::tuple<VkBuffer, VkDeviceMemory> MeshModel::CreateVertexBuffer(const std::vector<Vertex> &vertices)
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// Temporary, host-visible buffer that resides on the CPU
	auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map the buffer memory into the CPU accessible memory
	void *data;
	vkMapMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize); // Copy vertex data to the mapped memory
	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory);

	auto [vertexBuffer, vertexBufferMemory] = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(_vulkanCore->GetLogicalDevice(), stagingBuffer, vertexBuffer, bufferSize, _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue());

	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory, nullptr);

	return std::make_tuple(vertexBuffer, vertexBufferMemory);
}

std::tuple<VkBuffer, VkDeviceMemory> MeshModel::CreateIndexBuffer(const std::vector<uint32_t> &indices)
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	// Allocate separate buffer, one on the CPU and the other on the GPU.
	// Temporary, host-visible buffer that resides on the CPU
	auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map the buffer memory into CPU accessible memory
	void *data;
	vkMapMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize); // Copy index data to the mapped memory
	vkUnmapMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory);

	auto [indexBuffer, indexBufferMemory] = CreateBuffer(_vulkanCore->GetPhysicalDevice(), _vulkanCore->GetLogicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(_vulkanCore->GetLogicalDevice(), stagingBuffer, indexBuffer, bufferSize, _vulkanCore->GetCommandPool(), _vulkanCore->GetGraphicsQueue());

	vkDestroyBuffer(_vulkanCore->GetLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(_vulkanCore->GetLogicalDevice(), stagingBufferMemory, nullptr);

	return std::make_tuple(indexBuffer, indexBufferMemory);
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

std::tuple<std::vector<Vertex>, std::vector<uint32_t>> MeshModel::LoadOBJ(const std::string &OBJPath)
{
	tinyobj::attrib_t attribute; // Holds all of the positions, normals, and texture coordinates
	std::vector<tinyobj::shape_t> shapes; // Contains all of the separate objects and their faces
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &warn, &err, OBJPath.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex =
			{
				.pos =
				{
					attribute.vertices[3 * index.vertex_index + 0],
					attribute.vertices[3 * index.vertex_index + 1],
					attribute.vertices[3 * index.vertex_index + 2]
				},
				.color = { 1.0f, 1.0f, 1.0f },
				.texCoord =
				{
					attribute.texcoords[2 * index.texcoord_index + 0],
					1.0f - attribute.texcoords[2 * index.texcoord_index + 1] // Difference between the texture coordinate system
				}
			};

			if (!uniqueVertices.contains(vertex))
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}
			indices.push_back(uniqueVertices[vertex]);
		}
	}

	return std::make_tuple(std::move(vertices), std::move(indices));
}

void MeshModel::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if the image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(_vulkanCore->GetPhysicalDevice(), imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("The texture image format does not support linear blitting.");
	}

	// repeatedly blit a source image to a smaller one
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool());

	VkImageMemoryBarrier barrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;
	// (i - 1)'th layout
	// 1. VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	// 2. Blit
	// 3. VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	for (uint32_t i = 1; i < mipLevels; ++i)
	{
		barrier.subresourceRange.baseMipLevel = i - 1; // Process (i - 1)'th mip level
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Transition level (i - 1) to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier); // Wait for level i - 1 to be filled with previous blit command or vkCmdCopyBufferToImage

		// Blit area
		VkImageBlit blit =
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

		vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR); // Record the blit command - both the source and the destination images are same, because they are blitting between different mip levels.

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// This transition to the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL flag waits on the current blit command to finish.
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		// Divide the dimensions with a guarantee that they won't be 0
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition the last mipmap
	barrier.subresourceRange.baseMipLevel = mipLevels - 1; // Process the last mipmap
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	EndSingleTimeCommands(_vulkanCore->GetLogicalDevice(), _vulkanCore->GetCommandPool(), commandBuffer, _vulkanCore->GetGraphicsQueue());
}


