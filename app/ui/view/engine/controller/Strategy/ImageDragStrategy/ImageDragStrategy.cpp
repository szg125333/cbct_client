#include "ImageDragStrategy.h"
#include "interface/IViewController.h"
#include "interface/IViewRenderer.h"
#include "utils/ViewportUtils.h"

#include <vtkCamera.h>
#include <vtkRenderer.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindow.h>

#include <cmath>

ImageDragStrategy::ImageDragStrategy(IViewController* controller)
    : IInteractionStrategy(controller)
{
}

void ImageDragStrategy::HandleEvent(EventType        type,
    int              viewIndex,
    const EventData& data)
{
    if (!m_controller) return;
    if (m_activeViewIndex != -1 && m_activeViewIndex != viewIndex) return;

    auto* renderer = m_controller->GetRenderer(viewIndex);
    if (!renderer) return;

    switch (type) {

    case EventType::LeftPress: {
        m_isDragging = true;
        m_activeViewIndex = viewIndex;
        m_lastScreenX = data.mousePosX;
        m_lastScreenY = data.mousePosY;
        break;
    }

    case EventType::LeftMove: {
        if (!m_isDragging) break;

        const int pixDx = data.mousePosX - m_lastScreenX;
        const int pixDy = data.mousePosY - m_lastScreenY;
        if (pixDx == 0 && pixDy == 0) break;

        m_lastScreenX = data.mousePosX;
        m_lastScreenY = data.mousePosY;

        auto viewer = renderer->GetViewer();
        if (!viewer || !viewer->GetRenderer()) break;

        auto* camera = viewer->GetRenderer()->GetActiveCamera();
        if (!camera || !camera->GetParallelProjection()) break;

        const int* winSize = viewer->GetRenderWindow()->GetSize();
        if (winSize[0] <= 0 || winSize[1] <= 0) break;

        // 像素增量 → 世界坐标增量（OpenGL Y，不取反）
        auto delta = ViewportUtils::PixelDeltaToWorld(
            pixDx, pixDy, camera, winSize[0], winSize[1]);

        const double wdx = delta[0], wdy = delta[1], wdz = delta[2];

        // 相机反向平移（图像向拖动方向移动）
        double focal[3], pos[3];
        camera->GetFocalPoint(focal);
        camera->GetPosition(pos);
        camera->SetFocalPoint(focal[0] - wdx, focal[1] - wdy, focal[2] - wdz);
        camera->SetPosition(pos[0] - wdx, pos[1] - wdy, pos[2] - wdz);
        viewer->GetRenderer()->ResetCameraClippingRange();

        // 累计物理位移
        m_totalDx += wdx;
        m_totalDy += wdy;
        m_totalDz += wdz;

        const double total = std::sqrt(
            m_totalDx * m_totalDx + m_totalDy * m_totalDy + m_totalDz * m_totalDz);
        if (m_dragCb) m_dragCb(viewIndex, m_totalDx, m_totalDy, m_totalDz, total);

        renderer->RequestRender();
        break;
    }

    case EventType::LeftRelease: {
        m_isDragging = false;
        m_activeViewIndex = -1;
        break;
    }

    case EventType::RightPress: {
        ResetDisplacement();
        break;
    }

    default: break;
    }
}

void ImageDragStrategy::Clear(int)
{
    m_isDragging = false;
    m_activeViewIndex = -1;
    ResetDisplacement();
}

void ImageDragStrategy::ResetDisplacement()
{
    m_totalDx = m_totalDy = m_totalDz = 0.0;
    if (m_resetCb) m_resetCb();
}
