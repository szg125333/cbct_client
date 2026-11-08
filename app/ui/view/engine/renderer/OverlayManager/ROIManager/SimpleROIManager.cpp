#include "SimpleROIManager.h"

#include <vtkRenderer.h>
#include <vtkImageViewer2.h>
#include <vtkImageData.h>
#include <vtkCamera.h>
#include <vtkActor.h>
#include <vtkLineSource.h>
#include <vtkAppendPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolygon.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkCoordinate.h>
#include <vtkNew.h>

#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <limits>

// ============================================================
//  工具函数：获取三视图的轴索引
//
//  ax0  ax1  axF
//   X    Y    Z   → Axial    (XY 平面)
//   Y    Z    X   → Sagittal (YZ 平面)
//   X    Z    Y   → Coronal  (XZ 平面)
// ============================================================

static void GetPlaneAxes(ViewType viewType, int& ax0, int& ax1, int& axF)
{
    switch (viewType) {
    case ViewType::Axial:    ax0 = 0; ax1 = 1; axF = 2; break;
    case ViewType::Sagittal: ax0 = 1; ax1 = 2; axF = 0; break;
    case ViewType::Coronal:  ax0 = 0; ax1 = 2; axF = 1; break;
    default:                 ax0 = 0; ax1 = 1; axF = 2; break;
    }
}

// ============================================================
//  构造 / 析构
// ============================================================

SimpleROIManager::SimpleROIManager() = default;
SimpleROIManager::~SimpleROIManager() { Shutdown(); }

// ============================================================
//  IOverlayFeature —— 生命周期
// ============================================================

void SimpleROIManager::Initialize(vtkRenderer* overlayRenderer)
{
    if (m_initialized || !overlayRenderer) return;
    m_overlayRenderer = overlayRenderer;
    m_initialized = true;
}

void SimpleROIManager::Shutdown()
{
    if (!m_initialized) return;
    ClearAllROI();
    m_overlayRenderer = nullptr;
    m_initialized = false;
}

void SimpleROIManager::SetVisible(bool visible)
{
    m_visible = visible;
    if (!m_initialized) return;

    for (auto& [id, roi] : m_rois) {
        for (auto& bl : roi.borderLines) {
            if (bl.actor) bl.actor->SetVisibility(visible);
        }
        if (roi.fillActor) roi.fillActor->SetVisibility(visible);
        for (auto& ca : roi.cornerActors) {
            if (ca) ca->SetVisibility(visible);
        }
        roi.label.SetVisible(visible);
    }
    for (auto& pl : m_previewLines) {
        if (pl.actor) pl.actor->SetVisibility(visible && m_isDrawing);
    }
}

void SimpleROIManager::SetColor(double /*r*/, double /*g*/, double /*b*/) {}

// ============================================================
//  IOverlayFeature —— 切片变化
// ============================================================

void SimpleROIManager::OnSliceChanged(vtkImageViewer2* viewer,
    int              slice,
    ViewType         viewType)
{
    if (!m_initialized || !viewer || !viewer->GetInput()) return;

    double spacing[3], origin[3];
    viewer->GetInput()->GetSpacing(spacing);
    viewer->GetInput()->GetOrigin(origin);

    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);

    for (auto& [id, roi] : m_rois) {
        if (!roi.isComplete || roi.viewType != viewType) continue;

        // 更新法线方向坐标
        const double newVal = origin[axF] + slice * spacing[axF];
        roi.corner1[axF] = newVal;
        roi.corner2[axF] = newVal;
        roi.slice = slice;

        roi.stats = ComputeStats(viewer, roi.corner1, roi.corner2,
            roi.viewType, slice);

        std::array<double, 3> corners[4];
        ComputeRectCorners(roi.corner1, roi.corner2, roi.viewType, corners);
        auto anchor = ComputeLabelAnchor(corners, roi.viewType);
        RedrawROI(roi, corners, anchor);
    }
}

// ============================================================
//  绘制接口
// ============================================================

void SimpleROIManager::BeginROI(const std::array<double, 3>& cornerWorld,
    ViewType viewType, int slice)
{
    if (!m_initialized) return;
    m_isDrawing = true;
    m_drawCorner1 = cornerWorld;
    m_drawView = viewType;
    m_drawSlice = slice;
}

void SimpleROIManager::UpdatePreview(const std::array<double, 3>& oppositeCorner)
{
    if (!m_initialized || !m_isDrawing) return;

    std::array<double, 3> corners[4];
    ComputeRectCorners(m_drawCorner1, oppositeCorner, m_drawView, corners);

    if (!m_previewInitialized) {
        for (int i = 0; i < 4; ++i) {
            m_previewLines[i].source = vtkSmartPointer<vtkLineSource>::New();
            m_previewLines[i].mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            m_previewLines[i].mapper->SetInputConnection(
                m_previewLines[i].source->GetOutputPort());
            m_previewLines[i].actor = vtkSmartPointer<vtkActor>::New();
            m_previewLines[i].actor->SetMapper(m_previewLines[i].mapper);
            m_previewLines[i].actor->GetProperty()->SetColor(1.0, 1.0, 0.0);
            m_previewLines[i].actor->GetProperty()->SetLineWidth(1.5f);
            m_previewLines[i].actor->GetProperty()->SetLineStipplePattern(0xF0F0);
            m_overlayRenderer->AddActor(m_previewLines[i].actor);
        }
        m_previewInitialized = true;
    }

    for (int i = 0; i < 4; ++i) {
        m_previewLines[i].source->SetPoint1(corners[i].data());
        m_previewLines[i].source->SetPoint2(corners[(i + 1) % 4].data());
        m_previewLines[i].source->Modified();
        m_previewLines[i].actor->SetVisibility(true);
    }
}

void SimpleROIManager::CommitROI(const std::array<double, 3>& oppositeCorner,
    vtkImageViewer2* viewer)
{
    if (!m_initialized || !m_isDrawing) return;

    for (auto& pl : m_previewLines) {
        if (pl.actor) pl.actor->SetVisibility(false);
    }
    m_isDrawing = false;

    const auto& c1 = m_drawCorner1;
    const auto& c2 = oppositeCorner;
    const double dx = c2[0] - c1[0], dy = c2[1] - c1[1], dz = c2[2] - c1[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 2.0) return;

    int id = NextId();
    m_lastId = id;

    auto& roi = m_rois[id];
    roi.id = id;
    roi.viewType = m_drawView;
    roi.slice = m_drawSlice;
    roi.isComplete = true;

    int ax0, ax1, axF;
    GetPlaneAxes(m_drawView, ax0, ax1, axF);

    for (int i = 0; i < 3; ++i) {
        if (i == axF) {
            roi.corner1[i] = c1[i];
            roi.corner2[i] = c1[i];
        }
        else {
            roi.corner1[i] = std::min(c1[i], c2[i]);
            roi.corner2[i] = std::max(c1[i], c2[i]);
        }
    }

    roi.stats = ComputeStats(viewer, roi.corner1, roi.corner2,
        roi.viewType, roi.slice);

    std::array<double, 3> corners[4];
    ComputeRectCorners(roi.corner1, roi.corner2, roi.viewType, corners);
    auto anchor = ComputeLabelAnchor(corners, roi.viewType);

    InitBorderLines(roi);
    UpdateBorderLines(roi, corners);
    InitFillActor(roi, corners);
    InitCornerActors(roi, corners);   // 内部调用 CreateCornerSquare(... roi.viewType)

    roi.label.scale = kLabelScale;
    roi.label.lineSpacingMm = kLineSpacingMm;
    roi.label.Initialize(m_overlayRenderer);
    roi.label.SetLines(BuildLabelLines(roi.stats));
    roi.label.SetAnchor(anchor, static_cast<int>(roi.viewType));
    roi.label.SetVisible(true);
}

void SimpleROIManager::CancelCurrentROI()
{
    if (!m_isDrawing) return;
    for (auto& pl : m_previewLines) {
        if (pl.actor) pl.actor->SetVisibility(false);
    }
    m_isDrawing = false;
}

// ============================================================
//  拖动接口
// ============================================================

RoiHitResult SimpleROIManager::HitTest(int screenX, int screenY) const
{
    if (!m_overlayRenderer) return {};

    for (const auto& [id, roi] : m_rois) {
        if (!roi.isComplete) continue;

        std::array<double, 3> corners[4];
        ComputeRectCorners(roi.corner1, roi.corner2, roi.viewType, corners);

        // 角点顺序：[0]左下 [1]右下 [2]右上 [3]左上
        constexpr RoiHitType kCornerTypes[4] = {
            RoiHitType::CornerBL,
            RoiHitType::CornerBR,
            RoiHitType::CornerTR,
            RoiHitType::CornerTL,
        };

        // 先检测四角（优先级高）
        for (int i = 0; i < 4; ++i) {
            double sx, sy;
            if (!WorldToScreen(corners[i], sx, sy)) continue;
            const double dx = screenX - sx, dy = screenY - sy;
            if (std::sqrt(dx * dx + dy * dy) <= kCornerTolerancePx) {
                return { id, kCornerTypes[i] };
            }
        }

        // 再检测矩形包围盒内部
        double minSX = std::numeric_limits<double>::max();
        double minSY = std::numeric_limits<double>::max();
        double maxSX = -std::numeric_limits<double>::max();
        double maxSY = -std::numeric_limits<double>::max();
        bool allValid = true;

        for (int i = 0; i < 4; ++i) {
            double sx, sy;
            if (!WorldToScreen(corners[i], sx, sy)) { allValid = false; break; }
            minSX = std::min(minSX, sx); minSY = std::min(minSY, sy);
            maxSX = std::max(maxSX, sx); maxSY = std::max(maxSY, sy);
        }
        if (!allValid) continue;

        const double tol = kEdgeTolerancePx;
        if (screenX >= minSX - tol && screenX <= maxSX + tol &&
            screenY >= minSY - tol && screenY <= maxSY + tol) {
            return { id, RoiHitType::Body };
        }
    }
    return {};
}

void SimpleROIManager::MoveROI(int roiId,
    const std::array<double, 3>& delta,
    vtkImageViewer2* viewer)
{
    auto it = m_rois.find(roiId);
    if (it == m_rois.end()) return;
    auto& roi = it->second;

    int ax0, ax1, axF;
    GetPlaneAxes(roi.viewType, ax0, ax1, axF);

    // 只在自由轴上平移，法线轴不动
    roi.corner1[ax0] += delta[ax0];  roi.corner2[ax0] += delta[ax0];
    roi.corner1[ax1] += delta[ax1];  roi.corner2[ax1] += delta[ax1];

    roi.stats = ComputeStats(viewer, roi.corner1, roi.corner2,
        roi.viewType, roi.slice);

    std::array<double, 3> corners[4];
    ComputeRectCorners(roi.corner1, roi.corner2, roi.viewType, corners);
    auto anchor = ComputeLabelAnchor(corners, roi.viewType);
    RedrawROI(roi, corners, anchor);
}

void SimpleROIManager::ResizeROI(int roiId,
    RoiHitType hitType,
    const std::array<double, 3>& newCornerWorld,
    vtkImageViewer2* viewer)
{
    auto it = m_rois.find(roiId);
    if (it == m_rois.end()) return;
    auto& roi = it->second;

    int ax0, ax1, axF;
    GetPlaneAxes(roi.viewType, ax0, ax1, axF);

    // corner1 = [ax0_min, ax1_min]，corner2 = [ax0_max, ax1_max]
    switch (hitType) {
    case RoiHitType::CornerBL: roi.corner1[ax0] = newCornerWorld[ax0]; roi.corner1[ax1] = newCornerWorld[ax1]; break;
    case RoiHitType::CornerBR: roi.corner2[ax0] = newCornerWorld[ax0]; roi.corner1[ax1] = newCornerWorld[ax1]; break;
    case RoiHitType::CornerTR: roi.corner2[ax0] = newCornerWorld[ax0]; roi.corner2[ax1] = newCornerWorld[ax1]; break;
    case RoiHitType::CornerTL: roi.corner1[ax0] = newCornerWorld[ax0]; roi.corner2[ax1] = newCornerWorld[ax1]; break;
    default: return;
    }

    // 防止翻转
    if (roi.corner1[ax0] > roi.corner2[ax0]) std::swap(roi.corner1[ax0], roi.corner2[ax0]);
    if (roi.corner1[ax1] > roi.corner2[ax1]) std::swap(roi.corner1[ax1], roi.corner2[ax1]);

    roi.stats = ComputeStats(viewer, roi.corner1, roi.corner2,
        roi.viewType, roi.slice);

    std::array<double, 3> corners[4];
    ComputeRectCorners(roi.corner1, roi.corner2, roi.viewType, corners);
    auto anchor = ComputeLabelAnchor(corners, roi.viewType);
    RedrawROI(roi, corners, anchor);
}

// ============================================================
//  删除接口
// ============================================================

void SimpleROIManager::DeleteLastROI()
{
    if (m_lastId < 0) return;
    auto it = m_rois.find(m_lastId);
    if (it == m_rois.end()) return;
    RemoveRoiActors(it->second);
    m_rois.erase(it);
    if (m_rois.empty()) {
        m_lastId = -1;
    }
    else {
        auto last_it = m_rois.begin();
        for (auto iter = m_rois.begin(); iter != m_rois.end(); ++iter) {
            last_it = iter;
        }
        m_lastId = last_it->first;
    }
}

void SimpleROIManager::ClearAllROI()
{
    for (auto& [id, roi] : m_rois) RemoveRoiActors(roi);
    m_rois.clear();
    m_lastId = -1;

    for (auto& pl : m_previewLines) {
        if (pl.actor && m_overlayRenderer)
            m_overlayRenderer->RemoveActor(pl.actor);
        pl = {};
    }
    m_previewInitialized = false;
    m_isDrawing = false;
}

// ============================================================
//  查询接口
// ============================================================

RoiStats SimpleROIManager::GetLastStats() const
{
    if (m_lastId < 0) return {};
    auto it = m_rois.find(m_lastId);
    return it != m_rois.end() ? it->second.stats : RoiStats{};
}

RoiStats SimpleROIManager::GetStats(int roiId) const
{
    auto it = m_rois.find(roiId);
    return it != m_rois.end() ? it->second.stats : RoiStats{};
}

std::vector<RoiStatsRecord> SimpleROIManager::GetStatsRecords() const
{
    std::vector<RoiStatsRecord> out;
    out.reserve(m_rois.size());
    for (const auto& kv : m_rois) {
        const RoiRecord& rec = kv.second;
        const RoiStats& st = rec.stats;
        if (!st.IsValid()) continue;  // 只导出有像素统计的 ROI

        RoiStatsRecord r;
        r.kind = RoiStatsRecord::Kind::Rect;
        r.id = rec.id;
        r.complete = rec.isComplete;
        r.area = st.area;
        r.perimeter = 0.0;  // 矩形 ROI 不算周长
        r.mean = st.mean;
        r.stdDev = st.stdDev;
        r.minVal = st.minVal;
        r.maxVal = st.maxVal;
        r.pixelCount = st.pixelCount;
        out.push_back(r);
    }
    return out;
}

// ============================================================
//  私有 —— 标签文字
// ============================================================

std::vector<std::string> SimpleROIManager::BuildLabelLines(const RoiStats& stats)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    auto fmt = [&](const char* lbl, double val, const char* unit = "") -> std::string {
        oss.str(""); oss.clear();
        oss << lbl << val << unit;
        return oss.str();
        };

    std::vector<std::string> lines;
    lines.push_back(fmt("Mean:  ", stats.mean, " HU"));
    lines.push_back(fmt("SD:    ", stats.stdDev, ""));
    lines.push_back(fmt("Min:   ", stats.minVal, " HU"));
    lines.push_back(fmt("Max:   ", stats.maxVal, " HU"));

    oss.str(""); oss.clear();
    oss << "Area:  " << std::fixed << std::setprecision(1) << stats.area << " mm2";
    lines.push_back(oss.str());

    oss.str(""); oss.clear();
    oss << "Pixels:" << stats.pixelCount;
    lines.push_back(oss.str());

    return lines;
}

// ============================================================
//  私有 —— 坐标计算
// ============================================================

void SimpleROIManager::ComputeRectCorners(const std::array<double, 3>& c1,
    const std::array<double, 3>& c2,
    ViewType viewType,
    std::array<double, 3> out[4]) const
{
    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);

    const double minA = std::min(c1[ax0], c2[ax0]);
    const double maxA = std::max(c1[ax0], c2[ax0]);
    const double minB = std::min(c1[ax1], c2[ax1]);
    const double maxB = std::max(c1[ax1], c2[ax1]);
    const double fixV = c1[axF];

    // [0]左下  [1]右下  [2]右上  [3]左上
    out[0] = {}; out[1] = {}; out[2] = {}; out[3] = {};
    out[0][ax0] = minA; out[0][ax1] = minB; out[0][axF] = fixV;
    out[1][ax0] = maxA; out[1][ax1] = minB; out[1][axF] = fixV;
    out[2][ax0] = maxA; out[2][ax1] = maxB; out[2][axF] = fixV;
    out[3][ax0] = minA; out[3][ax1] = maxB; out[3][axF] = fixV;
}

std::array<double, 3> SimpleROIManager::ComputeLabelAnchor(
    const std::array<double, 3> corners[4],
    ViewType viewType) const
{
    // 下边中点（corners[0] 和 corners[1] 中点）
    std::array<double, 3> mid = {
        (corners[0][0] + corners[1][0]) * 0.5,
        (corners[0][1] + corners[1][1]) * 0.5,
        (corners[0][2] + corners[1][2]) * 0.5,
    };

    // 各视图"屏幕向下"方向：
    //   Axial    → Y 轴负方向（ax1=Y，屏幕下方）
    //   Sagittal → Z 轴负方向（ax1=Z，屏幕下方）
    //   Coronal  → Z 轴负方向（ax1=Z，屏幕下方）
    switch (viewType) {
    case ViewType::Axial:    mid[1] -= kLabelOffsetMm; break;
    case ViewType::Sagittal: mid[2] -= kLabelOffsetMm; break;
    case ViewType::Coronal:  mid[2] -= kLabelOffsetMm; break;
    default: break;
    }
    return mid;
}

bool SimpleROIManager::WorldToScreen(const std::array<double, 3>& world,
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

// ============================================================
//  私有 —— 图元初始化 / 更新
// ============================================================

void SimpleROIManager::InitBorderLines(RoiRecord& roi)
{
    for (int i = 0; i < 4; ++i) {
        roi.borderLines[i].source = vtkSmartPointer<vtkLineSource>::New();
        roi.borderLines[i].mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        roi.borderLines[i].mapper->SetInputConnection(
            roi.borderLines[i].source->GetOutputPort());
        roi.borderLines[i].actor = vtkSmartPointer<vtkActor>::New();
        roi.borderLines[i].actor->SetMapper(roi.borderLines[i].mapper);
        roi.borderLines[i].actor->GetProperty()->SetColor(1.0, 1.0, 0.0);
        roi.borderLines[i].actor->GetProperty()->SetLineWidth(2.0f);
        m_overlayRenderer->AddActor(roi.borderLines[i].actor);
    }
}

void SimpleROIManager::UpdateBorderLines(RoiRecord& roi,
    const std::array<double, 3> corners[4])
{
    for (int i = 0; i < 4; ++i) {
        roi.borderLines[i].source->SetPoint1(corners[i].data());
        roi.borderLines[i].source->SetPoint2(corners[(i + 1) % 4].data());
        roi.borderLines[i].source->Modified();
    }
}

void SimpleROIManager::InitFillActor(RoiRecord& roi,
    const std::array<double, 3> corners[4])
{
    auto pts = vtkSmartPointer<vtkPoints>::New();
    auto poly = vtkSmartPointer<vtkPolygon>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    auto pd = vtkSmartPointer<vtkPolyData>::New();

    pts->SetNumberOfPoints(4);
    poly->GetPointIds()->SetNumberOfIds(4);
    for (int i = 0; i < 4; ++i) {
        pts->SetPoint(i, corners[i].data());
        poly->GetPointIds()->SetId(i, i);
    }
    cells->InsertNextCell(poly);
    pd->SetPoints(pts);
    pd->SetPolys(cells);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);

    roi.fillActor = vtkSmartPointer<vtkActor>::New();
    roi.fillActor->SetMapper(mapper);
    roi.fillActor->GetProperty()->SetColor(0.2, 0.5, 1.0);
    roi.fillActor->GetProperty()->SetOpacity(0.12);
    m_overlayRenderer->AddActor(roi.fillActor);
}

void SimpleROIManager::UpdateFillActor(RoiRecord& roi,
    const std::array<double, 3> corners[4])
{
    if (!roi.fillActor) return;
    auto* mapper = dynamic_cast<vtkPolyDataMapper*>(roi.fillActor->GetMapper());
    if (!mapper) return;
    auto* pd = dynamic_cast<vtkPolyData*>(mapper->GetInput());
    if (!pd) return;

    vtkPoints* pts = pd->GetPoints();
    for (int i = 0; i < 4; ++i) pts->SetPoint(i, corners[i].data());
    pts->Modified();
    pd->Modified();
}

void SimpleROIManager::InitCornerActors(RoiRecord& roi,
    const std::array<double, 3> corners[4])
{
    for (int i = 0; i < 4; ++i) {
        // 传入 roi.viewType，确保方块在正确平面内生成
        roi.cornerActors[i] = CreateCornerSquare(corners[i], roi.viewType);
        m_overlayRenderer->AddActor(roi.cornerActors[i]);
    }
}

void SimpleROIManager::UpdateCornerActors(RoiRecord& roi,
    const std::array<double, 3> corners[4])
{
    for (int i = 0; i < 4; ++i) {
        if (roi.cornerActors[i]) {
            m_overlayRenderer->RemoveActor(roi.cornerActors[i]);
            roi.cornerActors[i] = nullptr;
        }
        // 同样传入 roi.viewType
        roi.cornerActors[i] = CreateCornerSquare(corners[i], roi.viewType);
        m_overlayRenderer->AddActor(roi.cornerActors[i]);
    }
}

// ================================================================
//  CreateCornerSquare —— 核心修正
//
//  根据 viewType 确定矩形所在平面的两个自由轴（ax0, ax1）：
//    Axial    → ax0=X(0)  ax1=Y(1)  axF=Z(2)
//    Sagittal → ax0=Y(1)  ax1=Z(2)  axF=X(0)
//    Coronal  → ax0=X(0)  ax1=Z(2)  axF=Y(1)
//
//  方块四个角点在 ax0/ax1 上各偏移 ±halfSize，
//  axF（法线方向）保持与 center 相同，确保方块始终在视图平面内可见。
// ================================================================

vtkSmartPointer<vtkActor> SimpleROIManager::CreateCornerSquare(
    const std::array<double, 3>& center,
    ViewType                     viewType,
    double                       halfSize)
{
    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);

    // 在视图平面内生成 4 个角点（顺时针：左下 → 右下 → 右上 → 左上）
    std::array<double, 3> pts[4];
    for (auto& p : pts) p = center;    // 先初始化为 center，法线方向自动正确

    pts[0][ax0] -= halfSize;  pts[0][ax1] -= halfSize;  // 左下
    pts[1][ax0] += halfSize;  pts[1][ax1] -= halfSize;  // 右下
    pts[2][ax0] += halfSize;  pts[2][ax1] += halfSize;  // 右上
    pts[3][ax0] -= halfSize;  pts[3][ax1] += halfSize;  // 左上
    // pts[i][axF] = center[axF]（已由初始化保证，无需额外赋值）

    auto vtkPts = vtkSmartPointer<vtkPoints>::New();
    auto poly = vtkSmartPointer<vtkPolygon>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    auto pd = vtkSmartPointer<vtkPolyData>::New();

    vtkPts->SetNumberOfPoints(4);
    poly->GetPointIds()->SetNumberOfIds(4);
    for (int i = 0; i < 4; ++i) {
        vtkPts->SetPoint(i, pts[i].data());
        poly->GetPointIds()->SetId(i, i);
    }
    cells->InsertNextCell(poly);
    pd->SetPoints(vtkPts);
    pd->SetPolys(cells);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 0.5, 0.0);  // 橙色
    actor->GetProperty()->SetOpacity(0.9);
    return actor;
}

// ============================================================
//  私有 —— 全量重绘
// ============================================================

void SimpleROIManager::RedrawROI(RoiRecord& roi,
    const std::array<double, 3> corners[4],
    const std::array<double, 3>& anchor)
{
    UpdateBorderLines(roi, corners);
    UpdateFillActor(roi, corners);
    UpdateCornerActors(roi, corners);   // 内部传 roi.viewType，三视图均正确
    roi.label.SetLines(BuildLabelLines(roi.stats));
    roi.label.SetAnchor(anchor, static_cast<int>(roi.viewType));
}

// ============================================================
//  私有 —— 统计计算
// ============================================================

RoiStats SimpleROIManager::ComputeStats(vtkImageViewer2* viewer,
    const std::array<double, 3>& c1,
    const std::array<double, 3>& c2,
    ViewType viewType, int slice) const
{
    RoiStats result;
    if (!viewer || !viewer->GetInput()) return result;

    vtkImageData* img = viewer->GetInput();
    double spacing[3], origin[3];
    int    dims[3];
    img->GetSpacing(spacing);
    img->GetOrigin(origin);
    img->GetDimensions(dims);

    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);

    auto toIdx = [&](double world, int axis) -> int {
        int idx = static_cast<int>((world - origin[axis]) / spacing[axis] + 0.5);
        return std::max(0, std::min(idx, dims[axis] - 1));
        };

    const int minI0 = toIdx(std::min(c1[ax0], c2[ax0]), ax0);
    const int maxI0 = toIdx(std::max(c1[ax0], c2[ax0]), ax0);
    const int minI1 = toIdx(std::min(c1[ax1], c2[ax1]), ax1);
    const int maxI1 = toIdx(std::max(c1[ax1], c2[ax1]), ax1);

    double sum = 0, sumSq = 0;
    double minV = std::numeric_limits<double>::max();
    double maxV = -std::numeric_limits<double>::max();
    int count = 0;

    int ijk[3];
    ijk[axF] = slice;
    for (int i = minI0; i <= maxI0; ++i) {
        ijk[ax0] = i;
        for (int j = minI1; j <= maxI1; ++j) {
            ijk[ax1] = j;
            if (ijk[0] < 0 || ijk[0] >= dims[0] ||
                ijk[1] < 0 || ijk[1] >= dims[1] ||
                ijk[2] < 0 || ijk[2] >= dims[2]) continue;

            double val = img->GetScalarComponentAsDouble(ijk[0], ijk[1], ijk[2], 0);
            sum += val;
            sumSq += val * val;
            minV = std::min(minV, val);
            maxV = std::max(maxV, val);
            ++count;
        }
    }

    if (count == 0) return result;

    result.pixelCount = count;
    result.mean = sum / count;
    result.minVal = minV;
    result.maxVal = maxV;
    const double var = (sumSq / count) - (result.mean * result.mean);
    result.stdDev = (var > 0.0) ? std::sqrt(var) : 0.0;
    result.area = (maxI0 - minI0 + 1) * spacing[ax0]
        * (maxI1 - minI1 + 1) * spacing[ax1];
    return result;
}

// ============================================================
//  私有 —— Actor 移除
// ============================================================

void SimpleROIManager::RemoveRoiActors(RoiRecord& roi)
{
    if (!m_overlayRenderer) return;

    for (auto& bl : roi.borderLines) {
        if (bl.actor) m_overlayRenderer->RemoveActor(bl.actor);
        bl = {};
    }
    if (roi.fillActor) {
        m_overlayRenderer->RemoveActor(roi.fillActor);
        roi.fillActor = nullptr;
    }
    for (auto& ca : roi.cornerActors) {
        if (ca) { m_overlayRenderer->RemoveActor(ca); ca = nullptr; }
    }
    roi.label.Shutdown();
}
