#include "ModelBase.h"

ModelBase::~ModelBase()
{
	OnCleanUpSwapChain();
	OnCleanUpOthers();
}