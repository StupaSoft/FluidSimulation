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

class SimulatedSceneBase : public DelegateRegistrable<SimulatedSceneBase>
{
protected:
	glm::uvec3 _gridDimension = glm::uvec3(64, 64, 64);
	std::unique_ptr<BVH> _bvh = std::make_unique<BVH>();

	// Physical parameters
	std::shared_ptr<SimulationParameters> _simulationParameters = std::make_shared<SimulationParameters>();
	Delegate<void(const SimulationParameters &)> _onUpdateSimulationParameters;

	// Rendering
	ParticleRenderingMode _particleRenderingMode = ParticleRenderingMode::MarchingCubes;
	Delegate<void(ParticleRenderingMode)> _onSetParticleRenderingMode;

	std::unique_ptr<Billboards> _billboards = nullptr;
	std::unique_ptr<MarchingCubes> _marchingCubes = nullptr;

	// Prop
	std::vector<std::shared_ptr<MeshModel>> _propModels;

public:
	Billboards *GetBillboards() { return _billboards.get(); }
	MarchingCubes *GetMarchingCubes() { return _marchingCubes.get(); }

	virtual void InitializeLevel() { _bvh->Construct(); }
	virtual void InitializeParticles(float particleDistance, glm::vec2 xRange, glm::vec2 yRange, glm::vec2 zRange) = 0;
	void SetParticleRenderingMode(ParticleRenderingMode particleRenderingMode);
	void UpdateSimulationParameters(const SimulationParameters &simulationParameters);
	virtual void AddProp(const std::wstring &OBJPath, const std::wstring &texturePath = L"", bool isVisible = true, bool isCollidable = true, RenderMode renderMode = RenderMode::Triangle);

	// Reflect the particle status to the render system
	virtual void InitializeRenderers(const std::vector<Buffer> &inputBuffers, size_t particleCount);
	virtual void ApplyRenderMode(ParticleRenderingMode particleRenderingMode);
};