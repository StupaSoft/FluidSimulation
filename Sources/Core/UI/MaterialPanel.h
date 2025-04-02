#pragma once

#include <memory>

#include "PanelBase.h"
#include "RayTracerCompute.h"

class MaterialPanel : public PanelBase
{
private:
	std::shared_ptr<RayTracerCompute> _rayTracer = nullptr;

public:
	MaterialPanel(const std::shared_ptr<RayTracerCompute> &rayTracer);
	virtual void Draw() override;
};