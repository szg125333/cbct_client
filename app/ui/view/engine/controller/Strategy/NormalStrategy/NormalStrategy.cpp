#include "controller/Strategy/NormalStrategy/NormalStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/CrosshairManager/SimpleCrosshairManager.h"

#include <vtkImageData.h>
#include <vtkRenderer.h>

// ============================================================
//  事件分发
// ============================================================

void NormalStrategy::HandleEvent(EventType type, int viewIndex, const EventData& data)
{
    if (!m_controller) return;

    const int screenX = data.mousePosX;
    const int screenY = data.mousePosY;

    switch (type) {

        // ---- 滚轮：切换切片 ----
    case EventType::WheelForward:
        m_controller->ChangeSlice(viewIndex, +1);
        break;

    case EventType::WheelBackward:
        m_controller->ChangeSlice(viewIndex, -1);
        break;

        // ---- 左键按下：记录起始状态 ----
    case EventType::LeftPress: {
        m_lastMousePos[0] = screenX;
        m_lastMousePos[1] = screenY;
        m_dragStartWindow = m_controller->GetWindowWidth();
        m_dragStartLevel = m_controller->GetWindowLevel();
        m_dragStartWorldPos = m_controller->GetRenderer(viewIndex)
            ->PickWorldPosition(screenX, screenY);
        m_isDragging = true;

        // 左键按下时同时更新十字线定位
        LocateCrosshair(viewIndex, screenX, screenY);
        break;
    }

                             // ---- 左键拖拽：窗宽窗位 / 缩放 ----
    case EventType::LeftMove: {
        if (!m_isDragging) break;

        const int dx = screenX - m_lastMousePos[0];
        const int dy = screenY - m_lastMousePos[1];
        if (dx == 0 && dy == 0) break;

        m_lastMousePos[0] = screenX;
        m_lastMousePos[1] = screenY;

        if (data.ctrlPressed) {
            // Ctrl + 拖拽 = 以按下点为焦点缩放
            // dy > 0（向下）缩小，dy < 0（向上）放大
            const double factor = 1.0 + dy * 0.01;
            m_controller->Zoom(viewIndex, factor, m_dragStartWorldPos);
        }
        else {
            // 默认：调整窗宽（水平）和窗位（垂直）
            double newWindow = m_dragStartWindow + dx * kWindowSensitivity;
            double newLevel = m_dragStartLevel - dy * kLevelSensitivity;

            // 窗宽不允许小于 1（否则图像全白或全黑）
            newWindow = std::max(newWindow, 1.0);

            // 更新拖拽基准，避免累积误差
            m_dragStartWindow = newWindow;
            m_dragStartLevel = newLevel;

            m_controller->SetWindowLevel(newWindow, newLevel);
        }
        break;
    }

                            // ---- 左键释放：结束拖拽 ----
    case EventType::LeftRelease:
        m_isDragging = false;
        break;

    default:
        break;
    }
}

// ============================================================
//  清除
// ============================================================

void NormalStrategy::Clear(int viewIndex)
{
    if (!m_controller) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* overlayMgr = renderer->GetOverlayManager();
    if (!overlayMgr) return;

    auto* crosshair = overlayMgr->GetFeature<SimpleCrosshairManager>();
    if (crosshair) {
        crosshair->ClearAllMeasurement();
    }
}

// ============================================================
//  私有方法
// ============================================================

void NormalStrategy::ApplyWindowLevel(int /*viewIndex*/)
{
    // 已在 HandleEvent 内直接调用 SetWindowLevel，此方法保留备用
}

void NormalStrategy::LocateCrosshair(int viewIndex, int screenX, int screenY)
{
    const auto* image = m_controller->GetImage();
    if (!image) return;

    // 拾取点击位置的世界坐标
    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    const std::array<double, 3> worldPoint =
        renderer->PickWorldPosition(screenX, screenY);

    // 计算图像世界坐标范围（用于十字线端点）
    int    dims[3];
    double spacing[3], origin[3];
    const_cast<vtkImageData*>(image)->GetDimensions(dims);
    const_cast<vtkImageData*>(image)->GetSpacing(spacing);
    const_cast<vtkImageData*>(image)->GetOrigin(origin);

    std::array<double, 3> worldMin, worldMax;
    for (int j = 0; j < 3; ++j) {
        worldMin[j] = origin[j];
        worldMax[j] = origin[j] + (dims[j] - 1) * spacing[j];
    }

    // 更新三视图十字线并同步切片位置
    for (int i = 0; i < 3; ++i) {
        auto* r = m_controller->GetRenderer(i);
        if (!r) continue;

        auto* overlayMgr = r->GetOverlayManager();
        if (overlayMgr) {
            auto* crosshair = overlayMgr->GetFeature<SimpleCrosshairManager>();
            if (crosshair) {
                crosshair->UpdateCrosshair(worldPoint,
                    static_cast<ViewType>(i),
                    worldMin.data(),
                    worldMax.data());
            }
        }

        r->SetCurrentClickWorldPos(worldPoint);
        r->RequestRender();
    }

    m_controller->UpdateSliceByWorldPoint(worldPoint);
}
