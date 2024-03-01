#pragma once

#include "MarchingCubesCompute.h"
#include "MeshModel.h"

class MarchingCubes
{
private:
	std::shared_ptr<VulkanCore> _vulkanCore = nullptr;

	std::unique_ptr<MarchingCubesCompute> _compute = nullptr;

	std::unique_ptr<MeshModel> _meshModel = nullptr;
	std::shared_ptr<MeshObject> _meshObject = nullptr;

public:
	MarchingCubes(const std::shared_ptr<VulkanCore> &vulkanCore, size_t particleCount, const SimulationParameters &simulationParameters, const MarchingCubesGrid &marchingCubesGrid);

	void SetEnable(bool enable);
	MarchingCubesCompute *GetCompute() { return _compute.get(); }
};

