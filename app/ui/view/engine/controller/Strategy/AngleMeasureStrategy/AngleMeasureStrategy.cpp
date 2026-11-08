#include "AngleMeasureStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/AngleMeasureManager/SimpleAngleMeasureManager.h"

AngleMeasureStrategy::AngleMeasureStrategy(IViewController* controller)
    : AbstractMeasureStrategy(controller)
{
}

// ============================================================
//  事件分发
// ============================================================

void AngleMeasureStrategy::HandleEvent(EventType type,
    int viewIndex,
    const EventData& data)
{
    if (!m_controller) return;

    const int screenX = data.mousePosX;
    const int screenY = data.mousePosY;

    // 空闲且非编辑的移动：无预览/无拖拽，直接返回——
    // 否则每次 hover 都会走 IsInsideImage 的完整拾取，测量模式下明显卡顿
    if (type == EventType::LeftMove
        && m_step == MeasureStep::Idle
        && !m_isEditingExisting
        && m_editingMeasurementId == -1) {
        return;
    }

    // 边界检查：左键类事件要求点在图像内；右键取消/键盘不受此限
    // （否则在图像外右键或按 Escape 会被静默吞掉，测量无法取消）
    const bool isMouseEvent = type == EventType::LeftPress
        || type == EventType::LeftMove
        || type == EventType::LeftRelease;
    if (isMouseEvent && !IsInsideImage(viewIndex, screenX, screenY)) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* angleFeature = overlayMgr->GetFeature<SimpleAngleMeasureManager>();
    if (!angleFeature) return;

    switch (type) {

        // ---- 左键点击：逐步放置三个端点 ----
    case EventType::LeftPress: {
        if (!m_isEditingExisting) {
            auto worldPos = renderer->PickWorldPosition(screenX, screenY);

            if (m_step == MeasureStep::Idle) {
                // 视图锁定：一次测量只在一个视图内完成
                if (!TryLockView(viewIndex)) return;

                // 第一次点击：放置起始点
                m_firstWorldPos = worldPos;
                m_step = MeasureStep::StartPlaced;
                angleFeature->DrawStartPoint(m_firstWorldPos);
                renderer->RequestRender();
            }
            else if (m_step == MeasureStep::StartPlaced) {
                // 第二次点击：放置顶点
                m_vertexWorldPos = worldPos;
                m_step = MeasureStep::VertexPlaced;
                angleFeature->DrawMiddlePointAndStartToMiddleLine(m_vertexWorldPos);
                renderer->RequestRender();
            }
            else if (m_step == MeasureStep::VertexPlaced) {
                // 第三次点击：放置终止点，完成测量
                angleFeature->DrawEndPointAndMiddleToEndLine(worldPos);
                renderer->RequestRender();

                // 重置状态，允许继续下一次测量
                m_step = MeasureStep::Idle;
                UnlockView();
            }
        }
        else {
            // 编辑模式：拾取可拖拽端点
            auto editPoint = angleFeature->GetEditableAnglePoint(screenX, screenY);
            if (editPoint.measurementId != -1) {
                m_editingMeasurementId = editPoint.measurementId;
                m_editingPointRole = static_cast<int>(editPoint.role);
            }
        }
        break;
    }

                             // ---- 鼠标移动：预览线段 / 拖拽端点 ----
    case EventType::LeftMove: {
        if (!m_isEditingExisting) {
            auto currentPos = renderer->PickWorldPosition(screenX, screenY);

            if (m_step == MeasureStep::StartPlaced) {
                // 预览起始点 → 当前位置的连线
                angleFeature->PreviewStartToMiddleMeasurementLine(
                    m_firstWorldPos, currentPos);
                renderer->RequestRender();
            }
            else if (m_step == MeasureStep::VertexPlaced) {
                // 预览顶点 → 当前位置的连线
                angleFeature->PreviewMiddleToEndMeasurementLine(
                    m_vertexWorldPos, currentPos);
                renderer->RequestRender();
            }
        }
        else if (m_editingMeasurementId != -1) {
            // 拖拽已有端点
            auto newPos = renderer->PickWorldPosition(screenX, screenY);
            angleFeature->UpdateAngleMeasurementPoint(
                m_editingMeasurementId, static_cast<AnglePointRole>(m_editingPointRole), newPos);
            renderer->RequestRender();
        }
        break;
    }

                            // ---- 左键释放：结束拖拽编辑 ----
    case EventType::LeftRelease: {
        if (m_isEditingExisting) {
            m_editingMeasurementId = -1;
            m_editingPointRole = -1;
            UnlockView();
        }
        break;
    }

                               // ---- 右键：仅取消当前进行中的测量绘制（对齐原版）----
    case EventType::RightPress: {
        if (m_step != MeasureStep::Idle) {
            angleFeature->ClearCurrentMeasurement();
            renderer->RequestRender();
            m_step = MeasureStep::Idle;
            UnlockView();
        }
        break;
    }

                              // ---- 键盘：Ctrl 进入编辑（原版无 Escape 取消）----
    case EventType::KeyPress: {
        m_isEditingExisting = data.ctrlPressed;
        break;
    }

    case EventType::KeyRelease:
        m_isEditingExisting = data.ctrlPressed;
        if (!m_isEditingExisting) {
            m_editingMeasurementId = -1;
            m_editingPointRole = -1;
            UnlockView();
        }
        break;

    default:
        break;
    }
}

// ============================================================
//  清除
// ============================================================

void AngleMeasureStrategy::Clear(int viewIndex)
{
    if (!m_controller) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* angleFeature = overlayMgr->GetFeature<SimpleAngleMeasureManager>();
    if (angleFeature) {
        angleFeature->ClearAllMeasurement();
    }

    // 重置策略内部状态
    m_step = MeasureStep::Idle;
    m_isEditingExisting = false;
    m_editingMeasurementId = -1;
    m_editingPointRole = -1;
    UnlockView();
}
