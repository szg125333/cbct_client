#include "DistanceMeasureStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/DistanceMeasureManager/SimpleDistanceMeasureManager.h"

DistanceMeasureStrategy::DistanceMeasureStrategy(IViewController* controller)
    : AbstractMeasureStrategy(controller)
{
}

// ============================================================
//  事件分发
// ============================================================

void DistanceMeasureStrategy::HandleEvent(EventType type,
    int viewIndex,
    const EventData& data)
{
    if (!m_controller) return;

    const int screenX = data.mousePosX;
    const int screenY = data.mousePosY;

    // 空闲且非编辑的移动：无预览可更新，跳过完整拾取（否则 hover 卡顿）
    if (type == EventType::LeftMove
        && !m_hasFirstPoint
        && !m_isEditingExisting
        && m_editingMeasurementId == -1) {
        return;
    }

    // 边界检查：左键类事件要求点在图像内；右键取消/键盘不受此限
    const bool isMouseEvent = type == EventType::LeftPress
        || type == EventType::LeftMove
        || type == EventType::LeftRelease;
    if (isMouseEvent && !IsInsideImage(viewIndex, screenX, screenY)) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* distFeature = overlayMgr->GetFeature<SimpleDistanceMeasureManager>();
    if (!distFeature) return;

    switch (type) {

        // ---- 左键点击：放置端点 ----
    case EventType::LeftPress: {
        if (!m_isEditingExisting) {
            if (!m_hasFirstPoint) {
                // 第一次点击：记录起始端点
                // 视图锁定：一次测量只在一个视图内完成
                if (!TryLockView(viewIndex)) return;

                m_firstWorldPos = renderer->PickWorldPosition(screenX, screenY);
                m_hasFirstPoint = true;

                distFeature->DrawStartPoint(m_firstWorldPos);
                renderer->RequestRender();
            }
            else {
                // 第二次点击：完成测量
                auto endPos = renderer->PickWorldPosition(screenX, screenY);
                distFeature->DrawFinalMeasurementLine(m_firstWorldPos, endPos);
                renderer->RequestRender();

                // 重置状态，允许继续下一次测量
                m_hasFirstPoint = false;
                UnlockView();
            }
        }
        else {
            // 编辑模式：拾取可拖拽端点
            auto editPoint = distFeature->GetEditablePoint(screenX, screenY);
            if (editPoint.measurementId != -1) {
                m_editingMeasurementId = editPoint.measurementId;
                m_editingIsStartPoint = editPoint.isStart;
            }
        }
        break;
    }

                             // ---- 鼠标移动：预览或拖拽 ----
    case EventType::LeftMove: {
        if (!m_isEditingExisting) {
            if (m_hasFirstPoint) {
                // 实时预览测量线
                auto currentPos = renderer->PickWorldPosition(screenX, screenY);
                distFeature->PreviewMeasurementLine(m_firstWorldPos, currentPos);
                renderer->RequestRender();
            }
        }
        else if (m_editingMeasurementId != -1) {
            // 拖拽已有端点
            auto newPos = renderer->PickWorldPosition(screenX, screenY);
            distFeature->UpdateMeasurementPoint(
                m_editingMeasurementId, m_editingIsStartPoint, newPos);
            renderer->RequestRender();
        }
        break;
    }

                            // ---- 左键释放：结束拖拽编辑 ----
    case EventType::LeftRelease: {
        if (m_isEditingExisting) {
            m_editingMeasurementId = -1;
            UnlockView();
        }

        if (!m_hasFirstPoint) {
            m_editingMeasurementId = -1;
            UnlockView();
        }
        break;
    }

                               // ---- 右键：仅取消当前进行中的测量绘制（对齐原版）----
    case EventType::RightPress: {
        if (m_hasFirstPoint) {
            distFeature->ClearCurrentMeasurement();
            renderer->RequestRender();
            m_hasFirstPoint = false;
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

void DistanceMeasureStrategy::Clear(int viewIndex)
{
    if (!m_controller) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* distFeature = overlayMgr->GetFeature<SimpleDistanceMeasureManager>();
    if (distFeature) {
        distFeature->ClearAllMeasurement();
    }

    // 重置策略内部状态
    m_hasFirstPoint = false;
    m_isEditingExisting = false;
    m_editingMeasurementId = -1;
    UnlockView();
}
