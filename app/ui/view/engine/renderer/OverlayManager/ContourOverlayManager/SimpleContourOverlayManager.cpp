#include "SimpleContourOverlayManager.h"

#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkImageViewer2.h>
#include <vtkImageData.h>

#include <cmath>
#include <algorithm>

SimpleContourOverlayManager::SimpleContourOverlayManager() = default;
SimpleContourOverlayManager::~SimpleContourOverlayManager() { Shutdown(); }

void SimpleContourOverlayManager::Initialize(vtkRenderer* r) {
    if (m_initialized || !r) return;
    m_overlayRenderer = r;
    m_initialized = true;
}

void SimpleContourOverlayManager::SetVisible(bool v) {
    m_visible = v;
    for (auto& a : m_currentActors) if (a) a->SetVisibility(v ? 1 : 0);
}

void SimpleContourOverlayManager::SetColor(double, double, double) {}

void SimpleContourOverlayManager::Shutdown() {
    if (!m_initialized) return;
    removeAllActors();
    m_overlayRenderer = nullptr;
    m_initialized = false;
}

void SimpleContourOverlayManager::OnSliceChanged(
    vtkImageViewer2* viewer, int slice, ViewType viewType)
{
    if (!m_initialized || !viewer || !m_rtData || !m_visible) return;

    auto* v = const_cast<vtkImageViewer2*>(viewer);
    vtkImageData* image = v->GetInput();
    if (!image) return;

    removeAllActors();

    double spacing[3], origin[3];
    image->GetSpacing(spacing);
    image->GetOrigin(origin);

    switch (viewType) {
    case ViewType::Axial: {
        double sliceZ = origin[2] + slice * spacing[2];
        updateAxialContours(sliceZ, spacing[2] / 2.0);
        break;
    }
    case ViewType::Sagittal: {
        double sliceX = origin[0] + slice * spacing[0];
        updateNonAxialContours(0, sliceX, spacing[0] / 2.0);
        break;
    }
    case ViewType::Coronal: {
        double sliceY = origin[1] + slice * spacing[1];
        updateNonAxialContours(1, sliceY, spacing[1] / 2.0);
        break;
    }
    default: break;
    }
}

void SimpleContourOverlayManager::SetRTStructureData(std::shared_ptr<RTStructureData> data) {
    m_rtData = data;
    if (data) {
        for (auto& kv : data->rois) m_roiVisibility[kv.first] = true;
    }
}

void SimpleContourOverlayManager::SetROIVisible(int n, bool v) { m_roiVisibility[n] = v; }

void SimpleContourOverlayManager::ClearAllContours() { removeAllActors(); }

// ============================================================
//  Axial：直接用原始轮廓点画闭合折线
// ============================================================

void SimpleContourOverlayManager::updateAxialContours(double sliceZ, double tolerance) {
    for (const auto& roiPair : m_rtData->rois) {
        const RTStructureROI& roi = roiPair.second;
        auto visIt = m_roiVisibility.find(roiPair.first);
        if (visIt != m_roiVisibility.end() && !visIt->second) continue;

        for (const auto& contour : roi.contours) {
            if (std::abs(contour.z - sliceZ) > tolerance) continue;
            if (contour.points.size() < 3) continue;

            std::vector<std::array<double, 3>> pts = contour.points;
            for (auto& p : pts) p[2] = sliceZ;

            auto actor = createLineActor(pts, roi.color, true);
            if (actor) {
                m_overlayRenderer->AddActor(actor);
                m_currentActors.push_back(actor);
            }
        }
    }
}

// ============================================================
//  Sagittal/Coronal：构建左右边界线
//
//  逻辑：
//  1. 每层轮廓与切面求交，得到若干交点
//  2. 每层所有交点中，取 sortAxis 最小的 → left 边界点
//                      取 sortAxis 最大的 → right 边界点
//  3. 所有层的 left 点按 Z 排序，连成左边界折线
//     所有层的 right 点按 Z 排序，连成右边界折线
//  4. 顶层 left-right、底层 left-right 各连一条封口线
//
//  因为每层只取最外侧的两个点，不管中间有多少对交点，
//  left 永远是最左边，right 永远是最右边，不会跳跃 → 无飞线
// ============================================================

void SimpleContourOverlayManager::updateNonAxialContours(
    int cutAxis, double slicePos, double tolerance)
{
    int sortAxis = (cutAxis == 0) ? 1 : 0;

    for (const auto& roiPair : m_rtData->rois) {
        const RTStructureROI& roi = roiPair.second;
        auto visIt = m_roiVisibility.find(roiPair.first);
        if (visIt != m_roiVisibility.end() && !visIt->second) continue;

        // 收集每层的最左和最右交点
        // key = Z, value = {left, right}
        struct LeftRight {
            std::array<double, 3> left, right;
        };
        std::map<double, LeftRight> borderByZ;

        for (const auto& contour : roi.contours) {
            if (contour.points.size() < 3) continue;

            std::vector<std::array<double, 3>> intersections;
            computeIntersections(contour, cutAxis, slicePos, intersections);
            if (intersections.empty()) continue;

            for (auto& pt : intersections) pt[cutAxis] = slicePos;

            // 找该轮廓交点中 sortAxis 最小和最大的
            auto minIt = std::min_element(intersections.begin(), intersections.end(),
                [sortAxis](const auto& a, const auto& b) { return a[sortAxis] < b[sortAxis]; });
            auto maxIt = std::max_element(intersections.begin(), intersections.end(),
                [sortAxis](const auto& a, const auto& b) { return a[sortAxis] < b[sortAxis]; });

            double z = contour.z;
            auto it = borderByZ.find(z);
            if (it == borderByZ.end()) {
                // 该 Z 层首次出现
                borderByZ[z] = { *minIt, *maxIt };
            }
            else {
                // 该 Z 层已有记录（同一层可能有多条轮廓），取更外侧的
                if ((*minIt)[sortAxis] < it->second.left[sortAxis])
                    it->second.left = *minIt;
                if ((*maxIt)[sortAxis] > it->second.right[sortAxis])
                    it->second.right = *maxIt;
            }
        }

        if (borderByZ.size() < 2) continue;

        // Z 排序
        std::vector<double> sortedZ;
        for (auto& kv : borderByZ) sortedZ.push_back(kv.first);
        std::sort(sortedZ.begin(), sortedZ.end());

        // 构建闭合轮廓：left 从下到上 → right 从上到下 → 闭合
        std::vector<std::array<double, 3>> contourPts;

        // left 边界：Z 从小到大
        for (double z : sortedZ) {
            contourPts.push_back(borderByZ[z].left);
        }

        // right 边界：Z 从大到小（反序）
        for (int i = static_cast<int>(sortedZ.size()) - 1; i >= 0; --i) {
            contourPts.push_back(borderByZ[sortedZ[i]].right);
        }

        // 画闭合轮廓
        auto actor = createLineActor(contourPts, roi.color, true);
        if (actor) {
            m_overlayRenderer->AddActor(actor);
            m_currentActors.push_back(actor);
        }
    }
}

// ============================================================
//  计算闭合多边形与平面的交点
// ============================================================

void SimpleContourOverlayManager::computeIntersections(
    const RTContour& contour, int axis, double slicePos,
    std::vector<std::array<double, 3>>& outPoints)
{
    int n = static_cast<int>(contour.points.size());

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;

        double v0 = contour.points[i][axis];
        double v1 = contour.points[j][axis];

        // 线段是否跨越 slicePos
        if ((v0 <= slicePos && v1 > slicePos) || (v1 <= slicePos && v0 > slicePos)) {
            double dv = v1 - v0;
            if (std::abs(dv) < 1e-10) continue;

            double t = (slicePos - v0) / dv;
            t = std::max(0.0, std::min(1.0, t));

            std::array<double, 3> pt;
            for (int d = 0; d < 3; ++d) {
                pt[d] = contour.points[i][d] + t * (contour.points[j][d] - contour.points[i][d]);
            }
            outPoints.push_back(pt);
        }
    }
}

// ============================================================
//  创建 Actor
// ============================================================

vtkSmartPointer<vtkActor> SimpleContourOverlayManager::createLineActor(
    const std::vector<std::array<double, 3>>& points,
    const std::array<double, 3>& color, bool closed)
{
    if (points.size() < 2) return nullptr;

    auto vtkPts = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    int n = static_cast<int>(points.size());

    for (int i = 0; i < n; ++i)
        vtkPts->InsertNextPoint(points[i][0], points[i][1], points[i][2]);

    int numLines = closed ? n : (n - 1);
    for (int i = 0; i < numLines; ++i) {
        vtkIdType ids[2] = { i, (i + 1) % n };
        lines->InsertNextCell(2, ids);
    }

    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(vtkPts);
    pd->SetLines(lines);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetLineWidth(2.0);
    return actor;
}

vtkSmartPointer<vtkActor> SimpleContourOverlayManager::createSegmentActor(
    const std::array<double, 3>& p1, const std::array<double, 3>& p2,
    const std::array<double, 3>& color)
{
    std::vector<std::array<double, 3>> pts = { p1, p2 };
    return createLineActor(pts, color, false);
}

void SimpleContourOverlayManager::removeAllActors() {
    if (!m_overlayRenderer) return;
    for (auto& a : m_currentActors) if (a) m_overlayRenderer->RemoveActor(a);
    m_currentActors.clear();
}

std::vector<ROIDisplayInfo> SimpleContourOverlayManager::GetROIList() const {
    std::vector<ROIDisplayInfo> list;
    if (!m_rtData) return list;

    for (const auto& kv : m_rtData->rois) {
        ROIDisplayInfo info;
        info.roiNumber = kv.first;
        info.name = kv.second.name;
        info.color = kv.second.color;
        auto visIt = m_roiVisibility.find(kv.first);
        info.visible = (visIt == m_roiVisibility.end()) || visIt->second;
        list.push_back(info);
    }
    return list;
}