#include "GPUSimulatedScene.h"

GPUSimulatedScene::GPUSimulatedScene(const std::shared_ptr<VulkanCore> &vulkanCore) :
	SimulatedSceneBase(vulkanCore)
{
}

void GPUSimulatedScene::InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	// Create a compute simulation module
	_simulationCompute = SimulationCompute::Instantiate<SimulationCompute>(_vulkanCore, _gridDimension);
	_onUpdateSimulationParameters.AddListener
	(
		weak_from_this(),
		[this](const SimulationParameters &simulationParameters)
		{
			_simulationCompute->UpdateSimulationParameters(simulationParameters);
		},
		__FUNCTION__,
		__LINE__
	);

	// Initialize the level
	SimulatedSceneBase::InitializeLevel();
	_simulationCompute->InitializeLevel(_bvh->GetNodes());

	// Finally initialize the particles in the compute pipeline
	size_t xCount = std::ceil((xRange.g - xRange.r) / particleDistance);
	size_t yCount = std::ceil((yRange.g - yRange.r) / particleDistance);
	size_t zCount = std::ceil((zRange.g - zRange.r) / particleDistance);
	size_t particleCount = xCount * yCount * zCount;

	glm::vec3 startingPoint = glm::vec3(xRange.r, yRange.r, zRange.r);

	// Place particles
	std::vector<glm::vec3> positions(particleCount);
	for (size_t z = 0; z < zCount; ++z)
	{
		for (size_t y = 0; y < yCount; ++y)
		{
			for (size_t x = 0; x < xCount; ++x)
			{
				size_t particleIndex = z * (xCount * yCount) + y * xCount + x;
				positions[particleIndex] = startingPoint + glm::vec3(x, y, z) * particleDistance;
			}
		}
	}
	_simulationCompute->InitializeParticles(positions);

	// Initialize renderers (marching cubes and billboards)
	// We don't have to create multiple particle position buffers for frames in flight since those buffers will be only written and read by the GPU.
	// We just reuse one buffer multiple times.
	Buffer positionBuffer = _simulationCompute->GetPositionInputBuffer();
	std::vector<Buffer> _particlePositionInputBuffers(_vulkanCore->GetMaxFramesInFlight(), positionBuffer);
	InitializeRenderers(_particlePositionInputBuffers, particleCount);

	// Launch
	_simulationCompute->SetEnable(true);
	ApplyRenderMode(_particleRenderingMode);
	_onUpdateSimulationParameters.Invoke(*_simulationParameters);
}

void GPUSimulatedScene::AddProp(const std::string &OBJPath, const std::string &texturePath, bool isVisible, bool isCollidable, RenderMode renderMode)
{
	SimulatedSceneBase::AddProp(OBJPath, texturePath, isVisible, isCollidable, renderMode);
}
