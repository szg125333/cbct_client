#include "RulerLineStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/RulerLineManager/SimpleRulerLineManager.h"
#include "utils/ViewportUtils.h"

#include <vtkCamera.h>
#include <vtkImageViewer2.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

RulerLineStrategy::RulerLineStrategy(IViewController* controller)
    : IInteractionStrategy(controller)
{
}

// ============================================================
//  OnActivated — 切换到此模式时立即在三个视图生成刻度线
// ============================================================

void RulerLineStrategy::OnActivated()
{
    if (!m_controller) return;

    for (int i = 0; i < 3; ++i) {
        auto* renderer = m_controller->GetRenderer(i);
        if (!renderer) continue;

        auto* overlayMgr = renderer->GetOverlayManager();
        if (!overlayMgr) continue;

        auto* rulerMgr = overlayMgr->GetFeature<SimpleRulerLineManager>();
        if (!rulerMgr) continue;

        auto viewer = renderer->GetViewer();
        if (!viewer) continue;

        rulerMgr->PlaceAtImageCenter(
            viewer.Get(),
            renderer->GetCurrentViewType(),
            renderer->GetSlice());
        rulerMgr->SetVisible(true);
        renderer->RequestRender();
    }
}

// ============================================================
//  HandleEvent
// ============================================================

void RulerLineStrategy::HandleEvent(EventType        type,
    int              viewIndex,
    const EventData& data)
{
    if (!m_controller) return;

    // 视图锁定：一次拖动只在一个视图内
    if (m_activeViewIndex != -1 && m_activeViewIndex != viewIndex) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    switch (type) {

        // ================================================================
        //  左键按下：根据 Ctrl 状态决定本次拖动是缩放还是移线
        // ================================================================
    case EventType::LeftPress: {
        m_activeViewIndex = viewIndex;
        m_lastScreenX = data.mousePosX;
        m_lastScreenY = data.mousePosY;

        if (data.ctrlPressed) {
            // ── Ctrl 按下：进入缩放模式 ──────────────────────────────
            // 以按下点的世界坐标为缩放焦点，与 NormalStrategy 行为一致
            m_dragMode = DragMode::Zoom;
            m_zoomFocalPoint = renderer->PickWorldPosition(
                data.mousePosX, data.mousePosY);
        }
        else {
            // ── 普通按下：命中测试，决定移哪条线 ──────────────────────
            auto* overlayMgr = renderer->GetOverlayManager();
            if (!overlayMgr) { m_activeViewIndex = -1; break; }

            auto* rulerMgr = overlayMgr->GetFeature<SimpleRulerLineManager>();
            if (!rulerMgr) { m_activeViewIndex = -1; break; }

            auto hit = rulerMgr->HitTest(data.mousePosX, data.mousePosY);
            if (hit != SimpleRulerLineManager::HitLine::None) {
                m_dragMode = DragMode::MoveRuler;
                m_dragTarget = hit;
            }
            else {
                // 未命中刻度线，不处理拖动
                m_dragMode = DragMode::None;
                m_activeViewIndex = -1;
            }
        }
        break;
    }

                             // ================================================================
                             //  左键拖动：根据 LeftPress 时确定的模式执行操作
                             // ================================================================
    case EventType::LeftMove: {
        if (m_dragMode == DragMode::None) break;

        const int pixDx = data.mousePosX - m_lastScreenX;
        const int pixDy = data.mousePosY - m_lastScreenY;
        if (pixDx == 0 && pixDy == 0) break;

        m_lastScreenX = data.mousePosX;
        m_lastScreenY = data.mousePosY;

        if (m_dragMode == DragMode::Zoom) {
            // ── 缩放模式 ──────────────────────────────────────────────
            //
            // 与 NormalStrategy 完全相同的公式：
            //   zoomFactor = 1.0 + pixDy × kZoomSensitivity
            //
            // pixDy 是 OpenGL Y（向上为正）：
            //   鼠标上移 → pixDy > 0 → zoomFactor > 1 → 放大 ✓
            //   鼠标下移 → pixDy < 0 → zoomFactor < 1 → 缩小 ✓
            //
            // 注意：这里不使用 ViewportUtils，因为缩放只需要 Y 方向
            //       的像素增量，不需要转换为世界坐标。
            //
            const double zoomFactor = 1.0 + pixDy * kZoomSensitivity;
            if (zoomFactor <= 0.0) break;   // 防御性检查

            m_controller->Zoom(viewIndex, zoomFactor, m_zoomFocalPoint);
        }
        else if (m_dragMode == DragMode::MoveRuler) {
            // ── 移线模式 ──────────────────────────────────────────────
            auto* overlayMgr = renderer->GetOverlayManager();
            if (!overlayMgr) break;

            auto* rulerMgr = overlayMgr->GetFeature<SimpleRulerLineManager>();
            if (!rulerMgr) break;

            auto viewer = renderer->GetViewer();
            if (!viewer || !viewer->GetRenderer()) break;

            auto* camera = viewer->GetRenderer()->GetActiveCamera();
            if (!camera || !camera->GetParallelProjection()) break;

            const int* ws = viewer->GetRenderWindow()->GetSize();
            if (ws[0] <= 0 || ws[1] <= 0) break;

            // 像素增量 → 世界坐标增量（OpenGL Y，不取反）
            auto delta = ViewportUtils::PixelDeltaToWorld(
                pixDx, pixDy, camera, ws[0], ws[1]);

            if (m_dragTarget == SimpleRulerLineManager::HitLine::Horizontal)
                rulerMgr->MoveHorizontalLine(delta);
            else if (m_dragTarget == SimpleRulerLineManager::HitLine::Vertical)
                rulerMgr->MoveVerticalLine(delta);

            renderer->RequestRender();
        }
        break;
    }

                            // ================================================================
                            //  左键释放：重置所有拖动状态
                            // ================================================================
    case EventType::LeftRelease: {
        m_dragMode = DragMode::None;
        m_activeViewIndex = -1;
        m_dragTarget = SimpleRulerLineManager::HitLine::None;
        break;
    }

    default:
        break;
    }
}

// ============================================================
//  Clear — 切换到其他模式时隐藏刻度线
// ============================================================

void RulerLineStrategy::Clear(int viewIndex)
{
    if (!m_controller) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    auto* mgr = renderer->GetOverlayManager();
    if (!mgr) return;

    auto* ruler = mgr->GetFeature<SimpleRulerLineManager>();
    if (ruler) ruler->SetVisible(false);

    m_dragMode = DragMode::None;
    m_activeViewIndex = -1;
    m_dragTarget = SimpleRulerLineManager::HitLine::None;
}
