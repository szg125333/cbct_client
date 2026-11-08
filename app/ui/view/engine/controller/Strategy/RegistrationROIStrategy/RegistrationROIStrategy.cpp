#include "RegistrationROIStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/ROIManager/SimpleROIManager.h"

#include <vtkImageViewer2.h>

RegistrationROIStrategy::RegistrationROIStrategy(IViewController* controller)
    : AbstractMeasureStrategy(controller)
{
}

// ============================================================
//  事件分发
// ============================================================

void RegistrationROIStrategy::HandleEvent(EventType        type,
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

    auto* roiMgr = overlayMgr->GetFeature<SimpleROIManager>();
    if (!roiMgr) return;

    // 获取当前视图信息（绘制和统计需要）
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
        RoiHitResult hit = roiMgr->HitTest(screenX, screenY);

        if (hit.roiId != -1) {
            // ---- 命中已有 ROI ----
            m_dragRoiId = hit.roiId;
            m_dragHitType = hit.hitType;

            // 记录按下时的世界坐标作为拖动基准
            m_lastDragWorldPos = renderer->PickWorldPosition(screenX, screenY);

            m_phase = (hit.hitType == RoiHitType::Body) ? Phase::Moving
                : Phase::Resizing;
        }
        else {
            // ---- 未命中，开始绘制新 ROI ----
            // 边界检查：起点必须在图像范围内
            if (!IsInsideImage(viewIndex, screenX, screenY)) {
                UnlockView();
                break;
            }

            m_drawViewType = viewType;
            m_drawSlice = slice;
            m_phase = Phase::Drawing;

            auto startWorld = renderer->PickWorldPosition(screenX, screenY);
            roiMgr->BeginROI(startWorld, viewType, slice);
        }

        renderer->RequestRender();
        break;
    }

    // ================================================================
    //  左键拖拽：根据当前阶段执行不同操作
    // ================================================================
    case EventType::LeftMove: {
        if (m_phase == Phase::Idle) break;

        auto currentWorld = renderer->PickWorldPosition(screenX, screenY);

        if (m_phase == Phase::Drawing) {
            // 更新预览虚线框
            roiMgr->UpdatePreview(currentWorld);
        }
        else if (m_phase == Phase::Moving) {
            // 整体平移：计算当前帧与上一帧的世界坐标增量
            if (!viewer) break;

            std::array<double, 3> delta = {
                currentWorld[0] - m_lastDragWorldPos[0],
                currentWorld[1] - m_lastDragWorldPos[1],
                currentWorld[2] - m_lastDragWorldPos[2],
            };
            m_lastDragWorldPos = currentWorld;

            roiMgr->MoveROI(m_dragRoiId, delta, viewer.Get());
        }
        else if (m_phase == Phase::Resizing) {
            // 角点缩放：将命中的角点移动到当前鼠标位置
            if (!viewer) break;
            roiMgr->ResizeROI(m_dragRoiId, m_dragHitType,
                currentWorld, viewer.Get());
        }

        renderer->RequestRender();
        break;
    }

                            // ================================================================
                            //  左键释放：结束当前操作
                            // ================================================================
    case EventType::LeftRelease: {
        if (m_phase == Phase::Drawing) {
            // 完成绘制：固定矩形并计算统计
            if (viewer) {
                auto endWorld = renderer->PickWorldPosition(screenX, screenY);
                roiMgr->CommitROI(endWorld, viewer.Get());
            }
            else {
                roiMgr->CancelCurrentROI();
            }
        }
        // Moving / Resizing 阶段在 LeftMove 中已实时更新，释放时只需重置状态

        m_phase = Phase::Idle;
        m_dragRoiId = -1;
        m_dragHitType = RoiHitType::None;
        UnlockView();

        renderer->RequestRender();
        break;
    }

                               // ================================================================
                               //  右键：取消当前绘制
                               // ================================================================
    case EventType::RightPress: {
        if (m_phase == Phase::Drawing) {
            roiMgr->CancelCurrentROI();
            m_phase = Phase::Idle;
            UnlockView();
            renderer->RequestRender();
        }
        break;
    }

                              // ================================================================
                              //  键盘：Delete / Ctrl+Delete / Esc
                              // ================================================================
    case EventType::KeyPress: {
        const std::string& key = data.keySym;

        if (key == "Delete" || key == "BackSpace") {
            if (data.ctrlPressed) {
                roiMgr->ClearAllROI();
            }
            else {
                roiMgr->DeleteLastROI();
            }
            renderer->RequestRender();
        }
        else if (key == "Escape") {
            if (m_phase == Phase::Drawing) {
                roiMgr->CancelCurrentROI();
                m_phase = Phase::Idle;
                UnlockView();
                renderer->RequestRender();
            }
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

void RegistrationROIStrategy::Clear(int viewIndex)
{
    if (!m_controller) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* roiMgr = overlayMgr->GetFeature<SimpleROIManager>();
    if (roiMgr) {
        roiMgr->ClearAllROI();
    }

    m_phase = Phase::Idle;
    m_dragRoiId = -1;
    m_dragHitType = RoiHitType::None;
    m_drawViewType = ViewType::None;
    m_drawSlice = 0;
    UnlockView();
}