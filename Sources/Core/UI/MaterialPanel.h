#pragma once

#include <memory>

#include "PanelBase.h"
#include "MeshModel.h"

class MaterialPanel : public PanelBase
{
private:
	std::shared_ptr<Material> _material = nullptr;

public:
	MaterialPanel(const std::shared_ptr<Material> &material);
	virtual void Draw() override;
};