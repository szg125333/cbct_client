#include "SimpleCrosshairManager.h"

#include <vtkRenderer.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkImageViewer2.h>
#include <vtkImageData.h>

#include <cstring>

// ============================================================
//  构造 / 析构
// ============================================================

SimpleCrosshairManager::SimpleCrosshairManager() = default;
SimpleCrosshairManager::~SimpleCrosshairManager() { Shutdown(); }

// ============================================================
//  生命周期
// ============================================================

void SimpleCrosshairManager::Initialize(vtkRenderer* overlayRenderer)
{
    if (m_initialized || !overlayRenderer) return;
    m_overlayRenderer = overlayRenderer;

    // 创建水平线（横线）
    m_hLine = vtkSmartPointer<vtkLineSource>::New();
    m_hMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_hActor = vtkSmartPointer<vtkActor>::New();
    m_hMapper->SetInputConnection(m_hLine->GetOutputPort());
    m_hActor->SetMapper(m_hMapper);
    m_hActor->GetProperty()->SetColor(0.0, 1.0, 0.0);   // 绿色
    m_hActor->GetProperty()->SetLineWidth(1.5f);

    // 创建垂直线（竖线）
    m_vLine = vtkSmartPointer<vtkLineSource>::New();
    m_vMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_vActor = vtkSmartPointer<vtkActor>::New();
    m_vMapper->SetInputConnection(m_vLine->GetOutputPort());
    m_vActor->SetMapper(m_vMapper);
    m_vActor->GetProperty()->SetColor(0.0, 1.0, 0.0);
    m_vActor->GetProperty()->SetLineWidth(1.5f);

    // 初始隐藏，点击后才显示
    m_hActor->SetVisibility(false);
    m_vActor->SetVisibility(false);

    m_overlayRenderer->AddActor(m_hActor);
    m_overlayRenderer->AddActor(m_vActor);

    m_initialized = true;
}

void SimpleCrosshairManager::Shutdown()
{
    if (!m_initialized) return;

    if (m_overlayRenderer) {
        if (m_hActor) m_overlayRenderer->RemoveActor(m_hActor);
        if (m_vActor) m_overlayRenderer->RemoveActor(m_vActor);
    }

    m_overlayRenderer = nullptr;
    m_initialized = false;
    m_hasValidPoint = false;
}

// ============================================================
//  样式控制
// ============================================================

void SimpleCrosshairManager::SetVisible(bool visible)
{
    m_visible = visible;
    if (!m_initialized) return;

    if (m_hActor) m_hActor->SetVisibility(visible && m_hasValidPoint);
    if (m_vActor) m_vActor->SetVisibility(visible && m_hasValidPoint);
}

void SimpleCrosshairManager::SetColor(double r, double g, double b)
{
    if (!m_initialized) return;
    if (m_hActor) m_hActor->GetProperty()->SetColor(r, g, b);
    if (m_vActor) m_vActor->GetProperty()->SetColor(r, g, b);
}

// ============================================================
//  十字线更新
// ============================================================

void SimpleCrosshairManager::UpdateCrosshair(std::array<double, 3> worldPoint,
    ViewType viewType,
    const double worldMin[3],
    const double worldMax[3])
{
    if (!m_initialized || !m_overlayRenderer) return;

    // 缓存当前状态，供切片变化时重绘
    m_lastWorldPoint = worldPoint;
    std::memcpy(m_lastImageMin, worldMin, sizeof(double) * 3);
    std::memcpy(m_lastImageMax, worldMax, sizeof(double) * 3);
    m_hasValidPoint = true;

    double hP1[3], hP2[3], vP1[3], vP2[3];

    // 根据视图方向确定两条线的端点
    // 每个视图固定其法线方向的坐标，另两个轴决定十字线方向
    switch (viewType) {
    case ViewType::Axial: {
        // XY 平面，Z 固定
        const double z = worldPoint[2];
        hP1[0] = worldMin[0]; hP1[1] = worldPoint[1]; hP1[2] = z;
        hP2[0] = worldMax[0]; hP2[1] = worldPoint[1]; hP2[2] = z;
        vP1[0] = worldPoint[0]; vP1[1] = worldMin[1]; vP1[2] = z;
        vP2[0] = worldPoint[0]; vP2[1] = worldMax[1]; vP2[2] = z;
        break;
    }
    case ViewType::Sagittal: {
        // YZ 平面，X 固定
        const double x = worldPoint[0];
        hP1[0] = x; hP1[1] = worldMin[1]; hP1[2] = worldPoint[2];
        hP2[0] = x; hP2[1] = worldMax[1]; hP2[2] = worldPoint[2];
        vP1[0] = x; vP1[1] = worldPoint[1]; vP1[2] = worldMin[2];
        vP2[0] = x; vP2[1] = worldPoint[1]; vP2[2] = worldMax[2];
        break;
    }
    case ViewType::Coronal: {
        // XZ 平面，Y 固定
        const double y = worldPoint[1];
        hP1[0] = worldMin[0]; hP1[1] = y; hP1[2] = worldPoint[2];
        hP2[0] = worldMax[0]; hP2[1] = y; hP2[2] = worldPoint[2];
        vP1[0] = worldPoint[0]; vP1[1] = y; vP1[2] = worldMin[2];
        vP2[0] = worldPoint[0]; vP2[1] = y; vP2[2] = worldMax[2];
        break;
    }
    default:
        return;
    }

    m_hLine->SetPoint1(hP1);
    m_hLine->SetPoint2(hP2);
    m_hLine->Modified();

    m_vLine->SetPoint1(vP1);
    m_vLine->SetPoint2(vP2);
    m_vLine->Modified();

    m_hActor->SetVisibility(true);
    m_vActor->SetVisibility(true);
}

void SimpleCrosshairManager::ClearAllMeasurement()
{
    if (!m_initialized) return;

    if (m_hActor) m_hActor->SetVisibility(false);
    if (m_vActor) m_vActor->SetVisibility(false);

    m_hasValidPoint = false;
}

// ============================================================
//  切片变化响应
// ============================================================

void SimpleCrosshairManager::OnSliceChanged(vtkImageViewer2* viewer,
    int slice,
    ViewType viewType)
{
    if (!m_initialized || !viewer || !m_hasValidPoint) return;

    // 获取图像的 spacing 和 origin
    double spacing[3];
    double origin[3];
    viewer->GetInput()->GetSpacing(spacing);
    viewer->GetInput()->GetOrigin(origin);

    // 更新 m_lastWorldPoint 对应的分量
    switch (viewType) {
    case ViewType::Axial:    // Z 轴方向
        m_lastWorldPoint[2] = origin[2] + slice * spacing[2];
        break;
    case ViewType::Sagittal: // X 轴方向
        m_lastWorldPoint[0] = origin[0] + slice * spacing[0];
        break;
    case ViewType::Coronal:  // Y 轴方向
        m_lastWorldPoint[1] = origin[1] + slice * spacing[1];
        break;
    default:
        return;
    }

    // 获取图像的世界范围
    double bounds[6];
    viewer->GetInput()->GetBounds(bounds);
    double worldMin[3] = { bounds[0], bounds[2], bounds[4] };
    double worldMax[3] = { bounds[1], bounds[3], bounds[5] };

    // 更新十字线
    UpdateCrosshair(m_lastWorldPoint, viewType, worldMin, worldMax);
}
