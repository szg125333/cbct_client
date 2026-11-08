#pragma once

#include "../IOverlayFeature.h"
#include "common/ViewTypes.h"
#include "common/RenderViewState.h"


class vtkRenderer;

class IOverlayInfoManager : public IOverlayFeature {
public:
    virtual ~IOverlayInfoManager() = default;
    virtual void Update(const RenderViewState& state) = 0;
};