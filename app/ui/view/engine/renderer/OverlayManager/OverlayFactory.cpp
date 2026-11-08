#include "OverlayFactory.h"
#include "SimpleOverlayManager.h"
#include "CrosshairManager/SimpleCrosshairManager.h"
#include "DistanceMeasureManager/SimpleDistanceMeasureManager.h"
#include "AngleMeasureManager/SimpleAngleMeasureManager.h"
#include "OverlayInfoManager/SimpleOverlayInfoManager.h"
#include "ROIManager/SimpleROIManager.h"           // 新增
#include "FreehandROIManager/SimpleFreehandROIManager.h"   // 新增
#include "RulerLineManager/SimpleRulerLineManager.h"
#include "ContourOverlayManager/SimpleContourOverlayManager.h"   // 新增

std::unique_ptr<IOverlayManager> OverlayFactory::CreateDefault()
{
    auto manager = std::make_unique<SimpleOverlayManager>();

    // 注册顺序即渲染层级（先注册的先渲染，后注册的覆盖在上层）
    manager->RegisterFeature(std::make_unique<SimpleCrosshairManager>());
    manager->RegisterFeature(std::make_unique<SimpleDistanceMeasureManager>());
    manager->RegisterFeature(std::make_unique<SimpleAngleMeasureManager>());
    manager->RegisterFeature(std::make_unique<SimpleOverlayInfoManager>());
    manager->RegisterFeature(std::make_unique<SimpleROIManager>());
    manager->RegisterFeature(std::make_unique<SimpleFreehandROIManager>());
    manager->RegisterFeature(std::make_unique<SimpleRulerLineManager>());
    manager->RegisterFeature(std::make_unique<SimpleContourOverlayManager>());

    return manager;
}