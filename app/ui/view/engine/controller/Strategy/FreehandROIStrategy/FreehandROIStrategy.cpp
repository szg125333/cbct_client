#include "FreehandROIStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/FreehandROIManager/SimpleFreehandROIManager.h"

#include <vtkImageViewer2.h>

#include <cmath>

FreehandROIStrategy::FreehandROIStrategy(IViewController* controller)
    : AbstractMeasureStrategy(controller)
{
}

// ============================================================
//  事件分发
// ============================================================

void FreehandROIStrategy::HandleEvent(EventType        type,
    int              viewIndex,
    const EventData& data)
{
    if (!m_controller) return;

    const int screenX = data.mousePosX;
    const int screenY = data.mousePosY;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* freehandMgr = overlayMgr->GetFeature<SimpleFreehandROIManager>();
    if (!freehandMgr) return;

    const ViewType viewType = renderer->GetCurrentViewType();
    const int      slice = renderer->GetSlice();
    auto           viewer = renderer->GetViewer();

    switch (type) {

        // ================================================================
        //  左键按下：判断是开始新绘制还是拖动已有 ROI
        // ================================================================
    case EventType::LeftPress: {

        // 视图锁定：一次操作只在一个视图内
        if (!TryLockView(viewIndex)) return;

        // 先做命中测试
        auto hit = freehandMgr->HitTest(screenX, screenY);

        if (hit.roiId != -1) {
            // ── 命中已有 ROI，进入平移模式 ──────────────────────
            m_phase = Phase::Moving;
            m_dragRoiId = hit.roiId;
            m_lastDragPos = renderer->PickWorldPosition(screenX, screenY);
        }
        else {
            // ── 未命中，开始绘制新轮廓 ──────────────────────────
            if (!IsInsideImage(viewIndex, screenX, screenY)) {
                UnlockView();
                break;
            }
            m_phase = Phase::Drawing;
            m_drawViewType = viewType;
            m_drawSlice = slice;

            auto startWorld = renderer->PickWorldPosition(screenX, screenY);
            freehandMgr->BeginFreehand(startWorld, viewType, slice);
        }

        renderer->RequestRender();
        break;
    }

                             // ================================================================
                             //  左键拖动：根据当前阶段执行操作
                             // ================================================================
    case EventType::LeftMove: {
        if (m_phase == Phase::Drawing) {
            // 追加轨迹点
            if (!IsInsideImage(viewIndex, screenX, screenY)) break;
            auto currentWorld = renderer->PickWorldPosition(screenX, screenY);
            freehandMgr->AddPoint(currentWorld);
            renderer->RequestRender();
        }
        else if (m_phase == Phase::Moving) {
            // 整体平移 ROI
            if (!viewer) break;
            auto currentWorld = renderer->PickWorldPosition(screenX, screenY);
            if (std::isnan(currentWorld[0])) break;

            // 计算增量（平面内分量）
            std::array<double, 3> delta = {
                currentWorld[0] - m_lastDragPos[0],
                currentWorld[1] - m_lastDragPos[1],
                currentWorld[2] - m_lastDragPos[2],
            };
            m_lastDragPos = currentWorld;

            freehandMgr->MoveROI(m_dragRoiId, delta, viewer.Get());
            renderer->RequestRender();
        }
        break;
    }

                            // ================================================================
                            //  左键释放：结束当前操作
                            // ================================================================
    case EventType::LeftRelease: {
        if (m_phase == Phase::Drawing) {
            if (viewer) {
                freehandMgr->CommitFreehand(viewer.Get());
            }
            else {
                freehandMgr->CancelFreehand();
            }
            renderer->RequestRender();
        }
        // Moving 阶段在 LeftMove 中已实时更新，释放时只需重置状态

        m_phase = Phase::Idle;
        m_dragRoiId = -1;
        UnlockView();
        break;
    }

                               // ================================================================
                               //  右键：取消当前绘制
                               // ================================================================
    case EventType::RightPress: {
        if (m_phase == Phase::Drawing) {
            freehandMgr->CancelFreehand();
            renderer->RequestRender();
        }
        m_phase = Phase::Idle;
        UnlockView();
        break;
    }

                              // ================================================================
                              //  键盘：Delete / Ctrl+Delete / Esc
                              // ================================================================
    case EventType::KeyPress: {
        const std::string& key = data.keySym;

        if (key == "Delete" || key == "BackSpace") {
            if (data.ctrlPressed) {
                freehandMgr->ClearAllFreehand();
            }
            else {
                freehandMgr->DeleteLastFreehand();
            }
            renderer->RequestRender();
        }
        else if (key == "Escape" && m_phase == Phase::Drawing) {
            freehandMgr->CancelFreehand();
            renderer->RequestRender();
            m_phase = Phase::Idle;
            UnlockView();
        }
        break;
    }

    default:
        break;
    }
}

// ============================================================
//  清除
// ============================================================

void FreehandROIStrategy::Clear(int viewIndex)
{
    if (!m_controller) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* freehandMgr = overlayMgr->GetFeature<SimpleFreehandROIManager>();
    if (freehandMgr) {
        freehandMgr->ClearAllFreehand();
    }

    m_phase = Phase::Idle;
    m_dragRoiId = -1;
    m_drawViewType = ViewType::None;
    m_drawSlice = 0;
    UnlockView();
}
