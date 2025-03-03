#include "GPUSimulatedScene.h"

GPUSimulatedScene::GPUSimulatedScene()
{
	_simulationCompute = SimulationCompute::Instantiate<SimulationCompute>(_gridDimension);
}

void GPUSimulatedScene::Register()
{
	_onUpdateSimulationParameters.AddListener
	(
		weak_from_this(),
		[this](const SimulationParameters &simulationParameters)
		{
			_simulationCompute->UpdateSimulationParameters(simulationParameters);
		},
		PRIORITY_LOWEST,
		__FUNCTION__,
		__LINE__
	);
}

void GPUSimulatedScene::InitializeLevel()
{
	SimulatedSceneBase::InitializeLevel();

	_simulationCompute->InitializeLevel(_bvh->GetNodes());
}

void GPUSimulatedScene::InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange)
{
	// Initialize the particles in the compute pipeline
	size_t xCount = static_cast<size_t>(std::ceil((xRange.g - xRange.r) / particleDistance));
	size_t yCount = static_cast<size_t>(std::ceil((yRange.g - yRange.r) / particleDistance));
	size_t zCount = static_cast<size_t>(std::ceil((zRange.g - zRange.r) / particleDistance));
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
	std::vector<Buffer> _particlePositionInputBuffers(VulkanCore::Get()->GetMaxFramesInFlight(), positionBuffer);
	InitializeRenderers(_particlePositionInputBuffers, particleCount);

	// Launch
	_simulationCompute->SetEnable(true);
	ApplyRenderMode(_particleRenderingMode);
	_onUpdateSimulationParameters.Invoke(*_simulationParameters);
}
