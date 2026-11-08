#include "SimpleFreehandROIManager.h"

#include <vtkRenderer.h>
#include <vtkImageViewer2.h>
#include <vtkImageData.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyLine.h>
#include <vtkPolygon.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkCoordinate.h>
#include <vtkNew.h>

#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <limits>

// ============================================================
//  轴工具函数
//
//  三视图 → 视图平面轴对应：
//    Axial    → ax0=X(0)  ax1=Y(1)  axF=Z(2)   XY 平面
//    Sagittal → ax0=Y(1)  ax1=Z(2)  axF=X(0)   YZ 平面
//    Coronal  → ax0=X(0)  ax1=Z(2)  axF=Y(1)   XZ 平面
// ============================================================

void SimpleFreehandROIManager::GetPlaneAxes(ViewType vt,
    int& ax0, int& ax1, int& axF)
{
    switch (vt) {
    case ViewType::Axial:    ax0 = 0; ax1 = 1; axF = 2; break;
    case ViewType::Sagittal: ax0 = 1; ax1 = 2; axF = 0; break;
    case ViewType::Coronal:  ax0 = 0; ax1 = 2; axF = 1; break;
    default:                 ax0 = 0; ax1 = 1; axF = 2; break;
    }
}

// ============================================================
//  构造 / 析构
// ============================================================

SimpleFreehandROIManager::SimpleFreehandROIManager() = default;
SimpleFreehandROIManager::~SimpleFreehandROIManager() { Shutdown(); }

// ============================================================
//  IOverlayFeature —— 生命周期
// ============================================================

void SimpleFreehandROIManager::Initialize(vtkRenderer* overlayRenderer)
{
    if (m_initialized || !overlayRenderer) return;
    m_overlayRenderer = overlayRenderer;
    m_initialized = true;
}

void SimpleFreehandROIManager::Shutdown()
{
    if (!m_initialized) return;
    ClearAllFreehand();
    m_overlayRenderer = nullptr;
    m_initialized = false;
}

void SimpleFreehandROIManager::SetVisible(bool visible)
{
    m_visible = visible;
    if (!m_initialized) return;

    for (auto& [id, rec] : m_records) {
        if (rec.outlineActor) rec.outlineActor->SetVisibility(visible);
        if (rec.fillActor)    rec.fillActor->SetVisibility(visible);
        rec.label.SetVisible(visible);
    }
    if (m_previewActor) m_previewActor->SetVisibility(visible && m_isDrawing);
}

void SimpleFreehandROIManager::SetColor(double, double, double) {}

// ============================================================
//  IOverlayFeature —— 切片变化
// ============================================================

void SimpleFreehandROIManager::OnSliceChanged(vtkImageViewer2* viewer,
    int              slice,
    ViewType         viewType)
{
    if (!m_initialized || !viewer || !viewer->GetInput()) return;

    double spacing[3], origin[3];
    viewer->GetInput()->GetSpacing(spacing);
    viewer->GetInput()->GetOrigin(origin);

    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);
    const double newVal = origin[axF] + slice * spacing[axF];

    for (auto& [id, rec] : m_records) {
        if (!rec.isComplete || rec.viewType != viewType) continue;

        // 所有点的法线坐标投影到新切片
        for (auto& pt : rec.points) pt[axF] = newVal;
        rec.slice = slice;

        // 重算统计 + 重建图元
        rec.stats = ComputeStats(viewer, rec.points, rec.viewType, slice);
        RebuildActors(rec);
    }
}

// ============================================================
//  绘制接口
// ============================================================

void SimpleFreehandROIManager::BeginFreehand(
    const std::array<double, 3>& startWorld,
    ViewType viewType, int slice)
{
    if (!m_initialized) return;
    m_isDrawing = true;
    m_drawView = viewType;
    m_drawSlice = slice;
    m_drawPoints.clear();
    m_drawPoints.push_back(startWorld);
}

void SimpleFreehandROIManager::AddPoint(const std::array<double, 3>& worldPoint)
{
    if (!m_isDrawing || m_drawPoints.empty()) return;

    // 稀疏采样：距离上一点超过阈值才追加
    const auto& last = m_drawPoints.back();
    const double dx = worldPoint[0] - last[0];
    const double dy = worldPoint[1] - last[1];
    const double dz = worldPoint[2] - last[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < kMinSampleDistMm) return;

    m_drawPoints.push_back(worldPoint);
    UpdatePreviewOutline(m_drawPoints);
}

void SimpleFreehandROIManager::CommitFreehand(vtkImageViewer2* viewer)
{
    if (!m_isDrawing) return;
    if (m_previewActor) m_previewActor->SetVisibility(false);
    m_isDrawing = false;

    if (m_drawPoints.size() < 3) {
        m_drawPoints.clear();
        return;
    }

    int id = NextId();
    m_lastId = id;

    auto& rec = m_records[id];
    rec.id = id;
    rec.viewType = m_drawView;
    rec.slice = m_drawSlice;
    rec.isComplete = true;
    rec.points = std::move(m_drawPoints);

    rec.stats = ComputeStats(viewer, rec.points, rec.viewType, rec.slice);

    // 创建图元
    rec.outlineActor = CreateOutlineActor(rec.points);
    rec.fillActor = CreateFillActor(rec.points);
    m_overlayRenderer->AddActor(rec.outlineActor);
    m_overlayRenderer->AddActor(rec.fillActor);

    // 初始化多行标签
    auto anchor = ComputeLabelAnchor(rec.points, rec.viewType);
    rec.label.scale = kLabelScale;
    rec.label.lineSpacingMm = kLineSpacingMm;
    rec.label.Initialize(m_overlayRenderer);
    rec.label.SetLines(BuildLabelLines(rec.stats));
    rec.label.SetAnchor(anchor, static_cast<int>(rec.viewType));
    rec.label.SetVisible(true);
}

void SimpleFreehandROIManager::CancelFreehand()
{
    if (!m_isDrawing) return;
    if (m_previewActor) m_previewActor->SetVisibility(false);
    m_drawPoints.clear();
    m_isDrawing = false;
}

// ============================================================
//  拖动接口
// ============================================================

FreehandHitResult SimpleFreehandROIManager::HitTest(int screenX, int screenY) const
{
    for (const auto& [id, rec] : m_records) {
        if (!rec.isComplete) continue;
        if (IsScreenPointInROIBounds(screenX, screenY, rec)) {
            return { id, FreehandHitType::Outline };
        }
    }
    return {};
}

void SimpleFreehandROIManager::MoveROI(int roiId,
    const std::array<double, 3>& delta,
    vtkImageViewer2* viewer)
{
    auto it = m_records.find(roiId);
    if (it == m_records.end()) return;
    auto& rec = it->second;

    int ax0, ax1, axF;
    GetPlaneAxes(rec.viewType, ax0, ax1, axF);

    // 只在平面内平移（法线轴不动）
    for (auto& pt : rec.points) {
        pt[ax0] += delta[ax0];
        pt[ax1] += delta[ax1];
        // pt[axF] 保持不变
    }

    rec.stats = ComputeStats(viewer, rec.points, rec.viewType, rec.slice);
    RebuildActors(rec);
}

// ============================================================
//  删除
// ============================================================

void SimpleFreehandROIManager::DeleteLastFreehand()
{
    if (m_lastId < 0) return;
    auto it = m_records.find(m_lastId);
    if (it == m_records.end()) return;
    RemoveFreehandActors(it->second);
    m_records.erase(it);
    if (m_records.empty()) {
        m_lastId = -1;
    }
    else {
        auto last_it = m_records.begin();
        for (auto iter = m_records.begin(); iter != m_records.end(); ++iter) {
            last_it = iter;
        }
        m_lastId = last_it->first;
    }
}

void SimpleFreehandROIManager::ClearAllFreehand()
{
    for (auto& [id, rec] : m_records) RemoveFreehandActors(rec);
    m_records.clear();
    m_lastId = -1;

    if (m_previewActor && m_overlayRenderer)
        m_overlayRenderer->RemoveActor(m_previewActor);
    m_previewActor = nullptr;
    m_previewMapper = nullptr;
    m_previewPolyData = nullptr;
    m_previewInitialized = false;
    m_isDrawing = false;
    m_drawPoints.clear();
}

// ============================================================
//  查询
// ============================================================

FreehandStats SimpleFreehandROIManager::GetLastStats() const
{
    if (m_lastId < 0) return {};
    auto it = m_records.find(m_lastId);
    return it != m_records.end() ? it->second.stats : FreehandStats{};
}

// ============================================================
//  预览折线
// ============================================================

void SimpleFreehandROIManager::UpdatePreviewOutline(
    const std::vector<std::array<double, 3>>& pts)
{
    if (!m_overlayRenderer || pts.empty()) return;

    if (!m_previewInitialized) {
        m_previewPolyData = vtkSmartPointer<vtkPolyData>::New();
        m_previewMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        m_previewMapper->SetInputData(m_previewPolyData);
        m_previewActor = vtkSmartPointer<vtkActor>::New();
        m_previewActor->SetMapper(m_previewMapper);
        m_previewActor->GetProperty()->SetColor(1.0, 1.0, 0.0);
        m_previewActor->GetProperty()->SetLineWidth(2.0f);
        m_overlayRenderer->AddActor(m_previewActor);
        m_previewInitialized = true;
    }

    const vtkIdType n = static_cast<vtkIdType>(pts.size());

    auto vtkPts = vtkSmartPointer<vtkPoints>::New();
    vtkPts->SetNumberOfPoints(n);
    for (vtkIdType i = 0; i < n; ++i)
        vtkPts->SetPoint(i, pts[i].data());

    auto polyLine = vtkSmartPointer<vtkPolyLine>::New();
    polyLine->GetPointIds()->SetNumberOfIds(n);
    for (vtkIdType i = 0; i < n; ++i)
        polyLine->GetPointIds()->SetId(i, i);

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    cells->InsertNextCell(polyLine);

    m_previewPolyData->SetPoints(vtkPts);
    m_previewPolyData->SetLines(cells);
    m_previewPolyData->Modified();
    m_previewActor->SetVisibility(true);
}

// ============================================================
//  VTK 图元构建
//
//  修正说明：
//  v1 中 CreateOutlineActor / CreateFillActor 没有问题（直接使用世界坐标），
//  但 CreateFillActor 用的 vtkPolygon 需要法线方向正确才能被渲染器看到。
//
//  对于正交投影（医学影像标准），vtkPolygon 会根据相机方向背面剔除，
//  如果多边形法线朝向错误（背向相机），填充面不可见。
//
//  修正：关闭背面剔除（BackfaceCullingOff），确保三视图均可见。
//  轮廓线不受此影响（线无正反面）。
// ============================================================

vtkSmartPointer<vtkActor> SimpleFreehandROIManager::CreateOutlineActor(
    const std::vector<std::array<double, 3>>& pts)
{
    const vtkIdType n = static_cast<vtkIdType>(pts.size());

    auto vtkPts = vtkSmartPointer<vtkPoints>::New();
    vtkPts->SetNumberOfPoints(n);
    for (vtkIdType i = 0; i < n; ++i)
        vtkPts->SetPoint(i, pts[i].data());

    // 闭合折线：最后连回第一点
    auto polyLine = vtkSmartPointer<vtkPolyLine>::New();
    polyLine->GetPointIds()->SetNumberOfIds(n + 1);
    for (vtkIdType i = 0; i < n; ++i)
        polyLine->GetPointIds()->SetId(i, i);
    polyLine->GetPointIds()->SetId(n, 0);

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    cells->InsertNextCell(polyLine);

    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(vtkPts);
    pd->SetLines(cells);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 1.0, 0.0);   // 黄色
    actor->GetProperty()->SetLineWidth(2.0f);
    return actor;
}

vtkSmartPointer<vtkActor> SimpleFreehandROIManager::CreateFillActor(
    const std::vector<std::array<double, 3>>& pts)
{
    const vtkIdType n = static_cast<vtkIdType>(pts.size());

    auto vtkPts = vtkSmartPointer<vtkPoints>::New();
    auto polygon = vtkSmartPointer<vtkPolygon>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    auto pd = vtkSmartPointer<vtkPolyData>::New();

    vtkPts->SetNumberOfPoints(n);
    polygon->GetPointIds()->SetNumberOfIds(n);
    for (vtkIdType i = 0; i < n; ++i) {
        vtkPts->SetPoint(i, pts[i].data());
        polygon->GetPointIds()->SetId(i, i);
    }
    cells->InsertNextCell(polygon);
    pd->SetPoints(vtkPts);
    pd->SetPolys(cells);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.2, 0.5, 1.0);
    actor->GetProperty()->SetOpacity(0.15);
    // ★ 关键修正：关闭背面剔除，三视图（Sagittal/Coronal 法线不同）均可见
    actor->GetProperty()->BackfaceCullingOff();
    return actor;
}

// ============================================================
//  图元生命周期
// ============================================================

void SimpleFreehandROIManager::RebuildActors(FreehandRecord& rec)
{
    RemoveFreehandActors(rec);

    rec.outlineActor = CreateOutlineActor(rec.points);
    rec.fillActor = CreateFillActor(rec.points);
    m_overlayRenderer->AddActor(rec.outlineActor);
    m_overlayRenderer->AddActor(rec.fillActor);

    auto anchor = ComputeLabelAnchor(rec.points, rec.viewType);
    if (!rec.label.IsInitialized()) {
        rec.label.scale = kLabelScale;
        rec.label.lineSpacingMm = kLineSpacingMm;
        rec.label.Initialize(m_overlayRenderer);
    }
    rec.label.SetLines(BuildLabelLines(rec.stats));
    rec.label.SetAnchor(anchor, static_cast<int>(rec.viewType));
    rec.label.SetVisible(true);
}

void SimpleFreehandROIManager::RemoveFreehandActors(FreehandRecord& rec)
{
    if (!m_overlayRenderer) return;
    if (rec.outlineActor) { m_overlayRenderer->RemoveActor(rec.outlineActor); rec.outlineActor = nullptr; }
    if (rec.fillActor) { m_overlayRenderer->RemoveActor(rec.fillActor);    rec.fillActor = nullptr; }
    rec.label.Shutdown();
}

// ============================================================
//  命中测试
// ============================================================

bool SimpleFreehandROIManager::WorldToScreen(const std::array<double, 3>& world,
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

bool SimpleFreehandROIManager::IsScreenPointInROIBounds(
    int screenX, int screenY,
    const FreehandRecord& rec) const
{
    if (rec.points.empty()) return false;

    // 计算所有点的屏幕 AABB
    double minSX = std::numeric_limits<double>::max();
    double minSY = std::numeric_limits<double>::max();
    double maxSX = -std::numeric_limits<double>::max();
    double maxSY = -std::numeric_limits<double>::max();

    for (const auto& pt : rec.points) {
        double sx, sy;
        if (!WorldToScreen(pt, sx, sy)) continue;
        minSX = std::min(minSX, sx); minSY = std::min(minSY, sy);
        maxSX = std::max(maxSX, sx); maxSY = std::max(maxSY, sy);
    }

    // 命中检测：屏幕点在 AABB 内（含容差）
    const double tol = kHitTolerancePx;
    return (screenX >= minSX - tol && screenX <= maxSX + tol &&
        screenY >= minSY - tol && screenY <= maxSY + tol);
}

// ============================================================
//  标签
// ============================================================

std::array<double, 3> SimpleFreehandROIManager::ComputeLabelAnchor(
    const std::vector<std::array<double, 3>>& pts,
    ViewType viewType) const
{
    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);

    double minA = std::numeric_limits<double>::max();
    double maxA = -std::numeric_limits<double>::max();
    double minB = std::numeric_limits<double>::max();
    double fixV = pts.empty() ? 0.0 : pts[0][axF];

    for (const auto& p : pts) {
        minA = std::min(minA, p[ax0]);
        maxA = std::max(maxA, p[ax0]);
        minB = std::min(minB, p[ax1]);
    }

    std::array<double, 3> anchor = {};
    anchor[ax0] = (minA + maxA) * 0.5;
    anchor[ax1] = minB;
    anchor[axF] = fixV;

    // 向"屏幕下方"偏移（各视图法线不同）
    switch (viewType) {
    case ViewType::Axial:    anchor[1] -= kLabelOffsetMm; break;
    case ViewType::Sagittal: anchor[2] -= kLabelOffsetMm; break;
    case ViewType::Coronal:  anchor[2] -= kLabelOffsetMm; break;
    default: break;
    }
    return anchor;
}

std::vector<std::string> SimpleFreehandROIManager::BuildLabelLines(const FreehandStats& stats)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    auto fmt = [&](const char* lbl, double val, const char* unit = "") -> std::string {
        oss.str(""); oss.clear();
        oss << lbl << val << unit;
        return oss.str();
        };

    std::vector<std::string> lines;
    lines.push_back(fmt("Area:              ", stats.area, " mm2"));
    lines.push_back(fmt("Perimeter:         ", stats.perimeter, " mm"));
    lines.push_back(fmt("Mean Pixel Value:  ", stats.mean, ""));
    lines.push_back(fmt("Standard Deviation:", stats.stdDev, ""));
    lines.push_back(fmt("Min Pixel Value:   ", stats.minVal, ""));
    lines.push_back(fmt("Max Pixel Value:   ", stats.maxVal, ""));
    oss.str(""); oss.clear();
    oss << "Number Of Pixels:  " << stats.pixelCount;
    lines.push_back(oss.str());
    return lines;
}

// ============================================================
//  统计计算
// ============================================================

FreehandStats SimpleFreehandROIManager::ComputeStats(
    vtkImageViewer2* viewer,
    const std::vector<std::array<double, 3>>& pts,
    ViewType viewType, int slice) const
{
    FreehandStats result;
    if (!viewer || !viewer->GetInput() || pts.size() < 3) return result;

    vtkImageData* img = viewer->GetInput();
    double spacing[3], origin[3];
    int    dims[3];
    img->GetSpacing(spacing);
    img->GetOrigin(origin);
    img->GetDimensions(dims);

    int ax0, ax1, axF;
    GetPlaneAxes(viewType, ax0, ax1, axF);

    // AABB
    double minA = std::numeric_limits<double>::max(), maxA = -std::numeric_limits<double>::max();
    double minB = std::numeric_limits<double>::max(), maxB = -std::numeric_limits<double>::max();
    for (const auto& p : pts) {
        minA = std::min(minA, p[ax0]); maxA = std::max(maxA, p[ax0]);
        minB = std::min(minB, p[ax1]); maxB = std::max(maxB, p[ax1]);
    }

    auto toIdx = [&](double world, int axis) -> int {
        int idx = static_cast<int>((world - origin[axis]) / spacing[axis] + 0.5);
        return std::max(0, std::min(idx, dims[axis] - 1));
        };

    const int minI0 = toIdx(minA, ax0), maxI0 = toIdx(maxA, ax0);
    const int minI1 = toIdx(minB, ax1), maxI1 = toIdx(maxB, ax1);

    double sum = 0, sumSq = 0;
    double minV = std::numeric_limits<double>::max();
    double maxV = -std::numeric_limits<double>::max();
    int count = 0;

    int ijk[3];
    ijk[axF] = slice;

    for (int i = minI0; i <= maxI0; ++i) {
        ijk[ax0] = i;
        double px = origin[ax0] + i * spacing[ax0];
        for (int j = minI1; j <= maxI1; ++j) {
            ijk[ax1] = j;
            if (ijk[0] < 0 || ijk[0] >= dims[0] ||
                ijk[1] < 0 || ijk[1] >= dims[1] ||
                ijk[2] < 0 || ijk[2] >= dims[2]) continue;

            double py = origin[ax1] + j * spacing[ax1];
            if (!IsPointInPolygon(px, py, pts, ax0, ax1)) continue;

            double val = img->GetScalarComponentAsDouble(ijk[0], ijk[1], ijk[2], 0);
            sum += val; sumSq += val * val;
            minV = std::min(minV, val); maxV = std::max(maxV, val);
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
    result.perimeter = ComputePerimeter(pts);
    result.area = ComputeArea(pts, ax0, ax1, spacing[ax0], spacing[ax1]);
    return result;
}

// ============================================================
//  射线法
// ============================================================

bool SimpleFreehandROIManager::IsPointInPolygon(
    double px, double py,
    const std::vector<std::array<double, 3>>& poly,
    int ax0, int ax1)
{
    const int n = static_cast<int>(poly.size());
    int crossings = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const double xi = poly[i][ax0], yi = poly[i][ax1];
        const double xj = poly[j][ax0], yj = poly[j][ax1];
        if (((yi > py) != (yj > py))) {
            double xIntersect = xj + (py - yj) / (yi - yj) * (xi - xj);
            if (px < xIntersect) ++crossings;
        }
    }
    return (crossings % 2) == 1;
}

// ============================================================
//  周长 / 面积
// ============================================================

double SimpleFreehandROIManager::ComputePerimeter(
    const std::vector<std::array<double, 3>>& pts)
{
    const int n = static_cast<int>(pts.size());
    double peri = 0.0;
    for (int i = 0; i < n; ++i) {
        const auto& p0 = pts[i]; const auto& p1 = pts[(i + 1) % n];
        double dx = p1[0] - p0[0], dy = p1[1] - p0[1], dz = p1[2] - p0[2];
        peri += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return peri;
}

double SimpleFreehandROIManager::ComputeArea(
    const std::vector<std::array<double, 3>>& pts,
    int ax0, int ax1, double sp0, double sp1)
{
    const int n = static_cast<int>(pts.size());
    if (n < 3) return 0.0;
    const double ox = pts[0][ax0], oy = pts[0][ax1];
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        double xi = (pts[i][ax0] - ox) / sp0, yi = (pts[i][ax1] - oy) / sp1;
        double xj = (pts[(i + 1) % n][ax0] - ox) / sp0, yj = (pts[(i + 1) % n][ax1] - oy) / sp1;
        sum += xi * yj - xj * yi;
    }
    return std::abs(sum) * 0.5 * sp0 * sp1;
}
