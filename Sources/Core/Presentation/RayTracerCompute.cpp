#include "RayTracerCompute.h"

RayTracerCompute::RayTracerCompute(const Image &screen) :
	_screen(screen)
{
	std::tie(_screenWidth, _screenHeight) = VulkanCore::Get()->GetScreenSize();

	Memory memory = CreateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	_screenBuffer = CreateBuffer(sizeof(glm::uvec2), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_cameraBuffer = CreateBuffer(sizeof(glm::vec3), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_lightBuffer = CreateBuffer(sizeof(DirectionalLight), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	_targetSceneBuffer = CreateBuffer(sizeof(glm::vec4) * _screenWidth * _screenHeight, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_renderedSceneBuffer = CreateBuffer(sizeof(glm::vec4) * _screenWidth * _screenHeight, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_targetSphereBuffer = CreateBuffer(sizeof(Sphere), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_learningSphereBuffer = CreateBuffer(sizeof(Sphere), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	_targetMaterialBuffer = CreateBuffer(sizeof(Material), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	_learningMaterialBuffer = CreateBuffer(sizeof(Material), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_gradientBuffer = CreateBuffer(sizeof(Material), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_learningRateBuffer = CreateBuffer(sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	memory->Bind({ _screenBuffer, _cameraBuffer, _lightBuffer, _targetSceneBuffer, _renderedSceneBuffer, _targetSphereBuffer, _learningSphereBuffer, _targetMaterialBuffer, _learningMaterialBuffer, _gradientBuffer, _learningRateBuffer });

	glm::uvec2 screenSize(_screenWidth, _screenHeight);
	_screenBuffer->CopyFrom(&screenSize);
	_cameraBuffer->CopyFrom(VulkanCore::Get()->GetMainCamera().get());
	_lightBuffer->CopyFrom(VulkanCore::Get()->GetMainLight().get());

	// At least set the initial material so that glossiness doesn't fall into local minimum...
	Material initLearningMaterial
	{
		._color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
		._specularColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
		._glossiness = _material._glossiness
	};
	_learningMaterialBuffer->CopyFrom(&initLearningMaterial);

	// Spheres
	Sphere targetSphere
	{
		.pos = glm::vec4(-1.0f, 0.0f, -5.0f, 0.0f),
		.radius = 0.5f
	};

	Sphere learningSphere
	{
		.pos = glm::vec4(1.0f, 0.0f, -5.0f, 0.0f),
		.radius = 0.5f
	};

	_targetSphereBuffer->CopyFrom(&targetSphere);
	_learningSphereBuffer->CopyFrom(&learningSphere);

	auto rayTracerShader = ShaderManager::Get()->GetShaderAsset("DifferentiableRayTracer");
	_rayTracingDescriptor = CreateRayTracingDescriptors(rayTracerShader);
	_rayTracingPipeline = CreateComputePipeline(rayTracerShader->GetShaderModule(), _rayTracingDescriptor->GetDescriptorSetLayout());

	auto updaterShader = ShaderManager::Get()->GetShaderAsset("MaterialUpdater");
	_updaterDescriptor = CreateUpdaterDescriptors(updaterShader);
	_updaterPipeline = CreateComputePipeline(updaterShader->GetShaderModule(), _updaterDescriptor->GetDescriptorSetLayout());
}

RayTracerCompute::~RayTracerCompute()
{
	vkDeviceWaitIdle(VulkanCore::Get()->GetLogicalDevice());
}

void RayTracerCompute::SetLearningRate(float learningRate)
{
	_learningRate = learningRate;
	_learningRateBuffer->CopyFrom(&_learningRate);
}

void RayTracerCompute::RecordCommand(VkCommandBuffer computeCommandBuffer, size_t currentFrame)
{
	// Update material every frame
	_targetMaterialBuffer->CopyFrom(&_material);

	// Execute pipeline
	VkMemoryBarrier memoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
	};

	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _rayTracingPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _rayTracingPipeline->GetPipelineLayout(), 0, 1, &_rayTracingDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, _screenWidth, _screenHeight, 1);

	vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _updaterPipeline->GetPipeline());
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _updaterPipeline->GetPipelineLayout(), 0, 1, &_updaterDescriptor->GetDescriptorSets()[currentFrame], 0, 0);
	vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

	// Present
	_screen->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	_screen->CopyFrom(_renderedSceneBuffer, static_cast<uint32_t>(_screenWidth), static_cast<uint32_t>(_screenHeight));
	_screen->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

Descriptor RayTracerCompute::CreateRayTracingDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);

	descriptor->BindBuffer("screenSize", _screenBuffer);
	descriptor->BindBuffer("cameraPos", _cameraBuffer);
	descriptor->BindBuffer("light", _lightBuffer);

	descriptor->BindBuffer("targetScene", _targetSceneBuffer);
	descriptor->BindBuffer("renderedScene", _renderedSceneBuffer);

	descriptor->BindBuffer("targetSphere", _targetSphereBuffer);
	descriptor->BindBuffer("learningSphere", _learningSphereBuffer);

	descriptor->BindBuffer("targetMaterial", _targetMaterialBuffer);
	descriptor->BindBuffer("learningMaterial", _learningMaterialBuffer);
	descriptor->BindBuffer("gradientMaterial", _gradientBuffer);

	return descriptor;
}

Descriptor RayTracerCompute::CreateUpdaterDescriptors(const Shader &shader)
{
	auto descriptor = CreateDescriptor(shader);
	
	descriptor->BindBuffer("screenSize", _screenBuffer);
	descriptor->BindBuffer("learningRate", _learningRateBuffer);
	descriptor->BindBuffer("learningMaterial", _learningMaterialBuffer);
	descriptor->BindBuffer("gradientMaterial", _gradientBuffer);

	return descriptor;
}
