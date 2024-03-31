#include "CPUSimulatedScene.h"

void SimulatedSceneBase::SetPlay(bool play)
{
	_isPlaying = play;
	_onSetPlay.Invoke(play);
}

void SimulatedSceneBase::AddProp(const std::string &OBJPath, const std::string &texturePath, bool isVisible, bool isCollidable, RenderMode renderMode)
{
	auto obj = LoadOBJ(OBJPath);

	auto propModel = MeshModel::Instantiate<MeshModel>(_vulkanCore);
	propModel->LoadMesh(std::get<0>(obj), std::get<1>(obj));
	propModel->LoadPipeline("Shaders/StandardVertex.spv", "Shaders/StandardFragment.spv", renderMode);
	propModel->LoadTexture(texturePath);

	auto propObject = propModel->AddMeshObject();
	propObject->SetVisible(isVisible);
	propObject->SetCollidable(isCollidable);

	_propModels.emplace_back(std::move(propModel));
	_bvh->AddPropObject(propObject);
}

void SimulatedSceneBase::SetParticleRenderingMode(ParticleRenderingMode particleRenderingMode)
{
	_particleRenderingMode = particleRenderingMode;
	_onSetParticleRenderingMode.Invoke(particleRenderingMode);
}

void SimulatedSceneBase::InitializeRenderers(const std::vector<Buffer> &inputBuffers, size_t particleCount)
{
	// Initialize renderers
	_billboards = std::make_unique<Billboards>(_vulkanCore, inputBuffers, particleCount);
	_billboards->UpdateRadius(_simulationParameters->_particleRadius);

	MarchingCubesGrid marchingCubesGrid // Temp
	{
		._xRange{ -3.0f, 3.0f },
		._yRange{ -1.0f, 5.0f },
		._zRange{ -2.0f, 2.0f },
		._voxelInterval = 0.05f
	};

	_marchingCubes = std::make_unique<MarchingCubes>(_vulkanCore, inputBuffers, particleCount, marchingCubesGrid);
	_marchingCubes->GetCompute()->UpdateParticleProperty(*_simulationParameters);
	_marchingCubes->SetEnable(false);
	
	// Add callbacks
	_onUpdateSimulationParameters.AddListener
	(
		weak_from_this(),
		[this](const SimulationParameters &simulationParameters)
		{
			_marchingCubes->GetCompute()->UpdateParticleProperty(simulationParameters);
			_billboards->UpdateRadius(simulationParameters._particleRadius);
		},
		__FUNCTION__,
		__LINE__
	);

	_onSetPlay.AddListener
	(
		weak_from_this(),
		[this](bool play)
		{
			ApplyRenderMode(_particleRenderingMode, play);
		},
		__FUNCTION__,
		__LINE__
	);

	_onSetParticleRenderingMode.AddListener
	(
		weak_from_this(),
		[this](ParticleRenderingMode particleRenderingMode)
		{
			ApplyRenderMode(particleRenderingMode, _isPlaying);
		},
		__FUNCTION__,
		__LINE__
	);
}

void SimulatedSceneBase::UpdateSimulationParameters(const SimulationParameters &simulationParameters)
{
	*_simulationParameters = simulationParameters;
	_onUpdateSimulationParameters.Invoke(*_simulationParameters);
}

void SimulatedSceneBase::ApplyRenderMode(ParticleRenderingMode particleRenderingMode, bool play)
{
	bool isMarchingCubes = (particleRenderingMode == ParticleRenderingMode::MarchingCubes);

	_marchingCubes->SetEnable(isMarchingCubes && play);
	_billboards->SetEnable(!isMarchingCubes && play);
}