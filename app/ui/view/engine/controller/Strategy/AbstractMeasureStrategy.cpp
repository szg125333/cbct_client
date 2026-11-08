#include "AbstractMeasureStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"

bool AbstractMeasureStrategy::IsInsideImage(int viewIndex,
    int screenX,
    int screenY) const
{
    if (!m_controller) return false;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return false;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return false;

    return overlayMgr->IsWorldPointInImage(
        renderer->PickWorldPosition(screenX, screenY));
}
