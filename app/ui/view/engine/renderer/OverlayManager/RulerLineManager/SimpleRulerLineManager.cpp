#include "SimpleRulerLineManager.h"

#include <vtkRenderer.h>
#include <vtkImageViewer2.h>
#include <vtkImageData.h>
#include <vtkCamera.h>
#include <vtkActor.h>
#include <vtkFollower.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkLine.h>
#include <vtkVectorText.h>
#include <vtkProperty.h>
#include <vtkCoordinate.h>
#include <vtkNew.h>

#include <cmath>
#include <algorithm>
#include <limits>

// ============================================================
//  轴映射
// ============================================================

void SimpleRulerLineManager::GetPlaneAxes(ViewType vt, int& ax0, int& ax1, int& axF)
{
    switch (vt) {
    case ViewType::Axial:    ax0 = 0; ax1 = 1; axF = 2; break;
    case ViewType::Sagittal: ax0 = 1; ax1 = 2; axF = 0; break;
    case ViewType::Coronal:  ax0 = 0; ax1 = 2; axF = 1; break;
    default:                 ax0 = 0; ax1 = 1; axF = 2; break;
    }
}

SimpleRulerLineManager::SimpleRulerLineManager() = default;
SimpleRulerLineManager::~SimpleRulerLineManager() { Shutdown(); }

// ============================================================
//  IOverlayFeature
// ============================================================

void SimpleRulerLineManager::Initialize(vtkRenderer* r)
{
    if (m_initialized || !r) return;
    m_overlayRenderer = r;
    m_initialized = true;
}

void SimpleRulerLineManager::Shutdown()
{
    if (!m_initialized) return;
    ClearHLine();
    ClearVLine();
    m_overlayRenderer = nullptr;
    m_initialized = false;
    m_isPlaced = false;
}

void SimpleRulerLineManager::SetVisible(bool visible)
{
    m_visible = visible;
    if (!m_initialized) return;
    if (m_hActor) m_hActor->SetVisibility(visible);
    if (m_vActor) m_vActor->SetVisibility(visible);
    if (m_hLabel) m_hLabel->SetVisibility(visible);
    if (m_vLabel) m_vLabel->SetVisibility(visible);
}

void SimpleRulerLineManager::SetColor(double, double, double) {}

void SimpleRulerLineManager::OnSliceChanged(vtkImageViewer2* viewer,
    int slice, ViewType viewType)
{
    if (!m_initialized || !m_isPlaced || m_viewType != viewType) return;
    if (!viewer || !viewer->GetInput()) return;

    double spacing[3], origin[3];
    viewer->GetInput()->GetSpacing(spacing);
    viewer->GetInput()->GetOrigin(origin);

    double bounds[6]; viewer->GetInput()->GetBounds(bounds);
    m_imageMin[0] = bounds[0]; m_imageMin[1] = bounds[2]; m_imageMin[2] = bounds[4];
    m_imageMax[0] = bounds[1]; m_imageMax[1] = bounds[3]; m_imageMax[2] = bounds[5];

    int ax0, ax1, axF; GetPlaneAxes(viewType, ax0, ax1, axF);
    m_axFVal = origin[axF] + slice * spacing[axF];
    m_imageMin[axF] = m_imageMax[axF] = m_axFVal;
    m_slice = slice;

    RebuildHLine();
    RebuildVLine();
}

// ============================================================
//  放置
// ============================================================

void SimpleRulerLineManager::PlaceAtImageCenter(vtkImageViewer2* viewer,
    ViewType viewType, int slice)
{
    if (!m_initialized || !viewer || !viewer->GetInput()) return;

    vtkImageData* img = viewer->GetInput();
    double spacing[3], origin[3]; int dims[3];
    img->GetSpacing(spacing); img->GetOrigin(origin); img->GetDimensions(dims);

    double bounds[6]; img->GetBounds(bounds);
    m_imageMin[0] = bounds[0]; m_imageMin[1] = bounds[2]; m_imageMin[2] = bounds[4];
    m_imageMax[0] = bounds[1]; m_imageMax[1] = bounds[3]; m_imageMax[2] = bounds[5];

    int ax0, ax1, axF; GetPlaneAxes(viewType, ax0, ax1, axF);

    m_viewType = viewType;
    m_slice = slice;
    m_axFVal = origin[axF] + slice * spacing[axF];
    m_imageMin[axF] = m_imageMax[axF] = m_axFVal;

    m_hLinePos = origin[ax1] + (dims[ax1] - 1) * spacing[ax1] * 0.5;
    m_vLinePos = origin[ax0] + (dims[ax0] - 1) * spacing[ax0] * 0.5;

    m_isPlaced = true;
    RebuildHLine();
    RebuildVLine();
}

// ============================================================
//  命中测试
// ============================================================

SimpleRulerLineManager::HitLine SimpleRulerLineManager::HitTest(
    int screenX, int screenY) const
{
    if (!m_initialized || !m_isPlaced) return HitLine::None;

    int ax0, ax1, axF; GetPlaneAxes(m_viewType, ax0, ax1, axF);

    auto lineScreenDist = [&](bool isH) -> double {
        std::array<double, 3> p1 = {}, p2 = {};
        if (isH) {
            p1[ax0] = m_imageMin[ax0]; p1[ax1] = m_hLinePos; p1[axF] = m_axFVal;
            p2[ax0] = m_imageMax[ax0]; p2[ax1] = m_hLinePos; p2[axF] = m_axFVal;
        }
        else {
            p1[ax0] = m_vLinePos; p1[ax1] = m_imageMin[ax1]; p1[axF] = m_axFVal;
            p2[ax0] = m_vLinePos; p2[ax1] = m_imageMax[ax1]; p2[axF] = m_axFVal;
        }
        double s1x, s1y, s2x, s2y;
        if (!WorldToScreen(p1, s1x, s1y) || !WorldToScreen(p2, s2x, s2y))
            return std::numeric_limits<double>::max();
        double lx = s2x - s1x, ly = s2y - s1y, len2 = lx * lx + ly * ly;
        if (len2 < 1e-6)
            return std::hypot(screenX - s1x, screenY - s1y);
        double t = std::max(0.0, std::min(1.0,
            ((screenX - s1x) * lx + (screenY - s1y) * ly) / len2));
        return std::hypot(screenX - (s1x + t * lx), screenY - (s1y + t * ly));
        };

    double dH = lineScreenDist(true);
    double dV = lineScreenDist(false);
    bool hitH = dH <= kHitTolerancePx;
    bool hitV = dV <= kHitTolerancePx;

    if (hitH && hitV) return dH <= dV ? HitLine::Horizontal : HitLine::Vertical;
    if (hitH) return HitLine::Horizontal;
    if (hitV) return HitLine::Vertical;
    return HitLine::None;
}

void SimpleRulerLineManager::MoveHorizontalLine(const std::array<double, 3>& d)
{
    int ax0, ax1, axF; GetPlaneAxes(m_viewType, ax0, ax1, axF);
    m_hLinePos = std::max(m_imageMin[ax1],
        std::min(m_imageMax[ax1], m_hLinePos + d[ax1]));
    RebuildHLine();
}

void SimpleRulerLineManager::MoveVerticalLine(const std::array<double, 3>& d)
{
    int ax0, ax1, axF; GetPlaneAxes(m_viewType, ax0, ax1, axF);
    m_vLinePos = std::max(m_imageMin[ax0],
        std::min(m_imageMax[ax0], m_vLinePos + d[ax0]));
    RebuildVLine();
}

// ============================================================
//  重建
// ============================================================

void SimpleRulerLineManager::RebuildHLine()
{
    ClearHLine();
    BuildLine(true, m_hLinePos, m_hActor, m_hLabel, m_hLabelText);
    if (m_hActor) m_hActor->SetVisibility(m_visible);
    if (m_hLabel) m_hLabel->SetVisibility(m_visible);
}

void SimpleRulerLineManager::RebuildVLine()
{
    ClearVLine();
    BuildLine(false, m_vLinePos, m_vActor, m_vLabel, m_vLabelText);
    if (m_vActor) m_vActor->SetVisibility(m_visible);
    if (m_vLabel) m_vLabel->SetVisibility(m_visible);
}

// ============================================================
//  核心：BuildLine
//
//  生成一条全宽/全高的参考线，带 1mm 刻度，只有一个 "1mm" 标签。
//
//  刻度规格：
//    每 1mm 一个短刻（kTickShort = 1.5mm 高）
//    每 5mm 一个中刻（kTickMid  = 3.0mm 高）
//    每 10mm 一个长刻（kTickLong = 5.0mm 高）
//    刻度关于主线对称（向 tickAxis 两侧各伸出 h/2）
//    交叉点（step=0）不画刻度
//
//  唯一标签 "1mm"：
//    位置 = 交叉点沿 extAxis 正方向 1mm 处，再向 tickAxis 负方向偏移
//    即放在第一个短刻旁边，标注最小刻度单位
//
//  ★ 字体大小 = kLabelScale（默认 3.0mm），修改 .h 中的 kLabelScale 即可
// ============================================================

void SimpleRulerLineManager::BuildLine(
    bool isHorizontal,
    double linePos,
    vtkSmartPointer<vtkActor>& outActor,
    vtkSmartPointer<vtkFollower>& outLabel,
    vtkSmartPointer<vtkVectorText>& outLabelText)
{
    if (!m_overlayRenderer) return;

    int ax0, ax1, axF; GetPlaneAxes(m_viewType, ax0, ax1, axF);

    const int    extAxis = isHorizontal ? ax0 : ax1;   // 线延伸方向
    const int    tickAxis = isHorizontal ? ax1 : ax0;   // 刻度伸出方向
    const double extMin = m_imageMin[extAxis];
    const double extMax = m_imageMax[extAxis];
    const double zeroPos = isHorizontal ? m_vLinePos : m_hLinePos;

    auto pts = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    auto addSeg = [&](std::array<double, 3> p1, std::array<double, 3> p2) {
        auto a = pts->InsertNextPoint(p1.data());
        auto b = pts->InsertNextPoint(p2.data());
        auto seg = vtkSmartPointer<vtkLine>::New();
        seg->GetPointIds()->SetId(0, a);
        seg->GetPointIds()->SetId(1, b);
        cells->InsertNextCell(seg);
        };

    // ── 主线 ─────────────────────────────────────────────────────
    {
        std::array<double, 3> p1 = {}, p2 = {};
        p1[extAxis] = extMin; p1[tickAxis] = linePos; p1[axF] = m_axFVal;
        p2[extAxis] = extMax; p2[tickAxis] = linePos; p2[axF] = m_axFVal;
        addSeg(p1, p2);
    }

    // ── 刻度线（1mm 步进，从零点向两侧）─────────────────────────
    const int stepsNeg = static_cast<int>(std::floor(zeroPos - extMin));
    const int stepsPos = static_cast<int>(std::floor(extMax - zeroPos));

    for (int step = -stepsNeg; step <= stepsPos; ++step) {
        if (step == 0) continue;

        const int absDist = std::abs(step);

        // 刻度高度：10mm 长刻，5mm 中刻，1mm 短刻
        double h;
        if (absDist % 10 == 0) h = kTickLong;
        else if (absDist % 5 == 0) h = kTickMid;
        else                         h = kTickShort;

        std::array<double, 3> base = {};
        base[extAxis] = zeroPos + static_cast<double>(step);
        base[tickAxis] = linePos;
        base[axF] = m_axFVal;

        std::array<double, 3> tp1 = base, tp2 = base;
        tp1[tickAxis] -= h * 0.5;
        tp2[tickAxis] += h * 0.5;
        addSeg(tp1, tp2);
    }

    // ── 构建主线 + 刻度 Actor ─────────────────────────────────────
    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(pts); pd->SetLines(cells);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);

    outActor = vtkSmartPointer<vtkActor>::New();
    outActor->SetMapper(mapper);
    outActor->GetProperty()->SetColor(
        isHorizontal ? 0.0 : 0.6,
        isHorizontal ? 0.9 : 1.0,
        isHorizontal ? 1.0 : 0.0);   // 水平=青色，垂直=黄绿色
    outActor->GetProperty()->SetLineWidth(1.5f);
    m_overlayRenderer->AddActor(outActor);

    // ── 唯一标签 "1mm" ────────────────────────────────────────────
    //
    // 放置位置：交叉点沿 extAxis 正方向 +1mm 处（对应第一个短刻）
    //           再向 tickAxis 负方向偏移 (kTickShort/2 + kLabelOffset)
    //
    // 这样标签紧贴在第一个 1mm 刻度旁，清晰标注最小刻度单位。
    //
    // ★ 字体大小由 kLabelScale 控制（.h 文件中修改）
    //
    auto* camera = m_overlayRenderer->GetActiveCamera();
    if (camera) {
        outLabelText = vtkSmartPointer<vtkVectorText>::New();
        outLabelText->SetText("1mm");

        auto lMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        lMapper->SetInputConnection(outLabelText->GetOutputPort());

        outLabel = vtkSmartPointer<vtkFollower>::New();
        outLabel->SetMapper(lMapper);
        outLabel->SetScale(kLabelScale, kLabelScale, kLabelScale);
        outLabel->SetCamera(camera);
        outLabel->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色

        // 标签位置：第一个 1mm 刻度旁（extAxis +1，tickAxis 负偏移）
        std::array<double, 3> lpos = {};
        lpos[extAxis] = zeroPos + 1.0;                          // 第一个刻度位置
        lpos[tickAxis] = linePos - (kTickShort * 0.5 + kLabelOffset);  // 刻度线外侧
        lpos[axF] = m_axFVal;
        outLabel->SetPosition(lpos[0], lpos[1], lpos[2]);

        m_overlayRenderer->AddViewProp(outLabel);
    }
}

// ============================================================
//  清除
// ============================================================

void SimpleRulerLineManager::ClearHLine()
{
    if (!m_overlayRenderer) return;
    if (m_hActor) { m_overlayRenderer->RemoveActor(m_hActor);     m_hActor = nullptr; }
    if (m_hLabel) { m_overlayRenderer->RemoveViewProp(m_hLabel);  m_hLabel = nullptr; }
    m_hLabelText = nullptr;
}

void SimpleRulerLineManager::ClearVLine()
{
    if (!m_overlayRenderer) return;
    if (m_vActor) { m_overlayRenderer->RemoveActor(m_vActor);     m_vActor = nullptr; }
    if (m_vLabel) { m_overlayRenderer->RemoveViewProp(m_vLabel);  m_vLabel = nullptr; }
    m_vLabelText = nullptr;
}

bool SimpleRulerLineManager::WorldToScreen(const std::array<double, 3>& world,
    double& outX, double& outY) const
{
    if (!m_overlayRenderer) return false;
    vtkNew<vtkCoordinate> coord;
    coord->SetCoordinateSystemToWorld();
    coord->SetViewport(m_overlayRenderer);
    coord->SetValue(const_cast<double*>(world.data()));
    int* d = coord->GetComputedDisplayValue(m_overlayRenderer);
    outX = d[0]; outY = d[1];
    return true;
}
