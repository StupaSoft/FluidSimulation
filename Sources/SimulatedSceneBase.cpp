#include "CPUSimulatedScene.h"

const glm::vec3 SimulatedSceneBase::GRAVITY = glm::vec3(0.0f, -9.8f, 0.0f);

void SimulatedSceneBase::SetPlay(bool play)
{
	_isPlaying = play;
	_onSetPlay.Invoke(play);
}

void SimulatedSceneBase::SetParticleRenderingMode(ParticleRenderingMode particleRenderingMode)
{
	_particleRenderingMode = particleRenderingMode;
	_onSetParticleRenderingMode.Invoke(particleRenderingMode);
}

void SimulatedSceneBase::InitializeRenderSystems(const SimulationParameters &simulationParameters)
{
	if (_billboards == nullptr)
	{
		_billboards = std::make_unique<Billboards>(_vulkanCore, _particleCount, simulationParameters._particleRadius);
	}

	if (_marchingCubes == nullptr)
	{
		MarchingCubesGrid marchingCubesGrid
		{
			._xRange{ -3.0f, 3.0f },
			._yRange{ -3.0f, 5.0f },
			._zRange{ -3.0f, 3.0f },
			._voxelInterval = 0.05f
		};

		_marchingCubes = std::make_unique<MarchingCubes>(_vulkanCore, _particleCount, simulationParameters, marchingCubesGrid);
		_marchingCubes->SetEnable(false);
	}
}

void SimulatedSceneBase::UpdateSimulationParamters(const SimulationParameters &simulationParamters)
{
	if (_simulationParameters == nullptr) _simulationParameters = std::make_unique<SimulationParameters>();
	_onUpdateSimulationParameters.Invoke(simulationParamters);
	*_simulationParameters = simulationParamters;
}

void SimulatedSceneBase::ApplyRenderMode(ParticleRenderingMode particleRenderingMode, bool play)
{
	bool isMarchingCubes = (particleRenderingMode == ParticleRenderingMode::MarchingCubes);

	_marchingCubes->SetEnable(isMarchingCubes && play);
	_billboards->SetEnable(!isMarchingCubes && play);
}