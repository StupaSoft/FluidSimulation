#pragma once

#include <memory>

#include "PanelBase.h"
#include "MeshModel.h"

class MaterialPanel : public PanelBase
{
private:
	std::shared_ptr<MeshModel> _meshModel = nullptr;
	MeshModel::Material _material{};

public:
	MaterialPanel(const std::shared_ptr<MeshModel> &meshModel);
	virtual void Draw() override;
};