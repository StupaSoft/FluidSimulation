#pragma once

#include <omp.h>

#include "VulkanCore.h"
#include "BVH.h"
#include "SimulationParameters.h"
#include "Delegate.h"

#include "MeshModel.h"
#include "Billboards.h"
#include "MarchingCubes.h"

enum class ColliderRenderMode
{
	Solid,
	Invisible,
	Wireframe
};

enum class ParticleRenderingMode
{
	Billboards,
	MarchingCubes
};

class SimulatedSceneBase
{
protected:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

	bool _isPlaying = false;
	Delegate<void(bool)> _onSetPlay;

	// Particles in the data structure
	size_t _particleCount = 0;

	glm::uvec3 _gridResolution = glm::uvec3(100, 100, 100);
	std::unique_ptr<BVH> _bvh = std::make_unique<BVH>();

	// Physics parameters
	static const glm::vec3 GRAVITY;
	std::unique_ptr<SimulationParameters> _simulationParameters = nullptr;
	Delegate<void(const SimulationParameters &)> _onUpdateSimulationParameters;

	// Rendering
	ParticleRenderingMode _particleRenderingMode = ParticleRenderingMode::MarchingCubes;
	Delegate<void(ParticleRenderingMode)> _onSetParticleRenderingMode;

	std::unique_ptr<Billboards> _billboards = nullptr;
	std::unique_ptr<MarchingCubes> _marchingCubes = nullptr;

	// Prop
	std::vector<std::unique_ptr<MeshModel>> _propModels;

public:
	SimulatedSceneBase(const std::shared_ptr<VulkanCore> &vulkanCore) : _vulkanCore(vulkanCore) {}

	void UpdateSimulationParamters(const SimulationParameters &simulationParamters);
	void SetPlay(bool play);
	bool IsPlaying() { return _isPlaying; }

	void SetParticleRenderingMode(ParticleRenderingMode particleRenderingMode);
	Billboards *GetBillboards() { return _billboards.get(); }
	MarchingCubes *GetMarchingCubes() { return _marchingCubes.get(); }

	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) = 0;
	virtual void AddProp(const std::string &OBJPath, const std::string &texturePath = "", bool isVisible = true, bool isCollidable = true) = 0;
	virtual void Update(float deltaSecond) = 0;
	
protected:
	// Reflect the particle status to the render system
	void InitializeRenderSystems(const SimulationParameters &simulationParameters);
	void ApplyRenderMode(ParticleRenderingMode particleRenderingMode, bool play);
};