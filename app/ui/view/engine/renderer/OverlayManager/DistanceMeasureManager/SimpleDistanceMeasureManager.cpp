#include "SimpleDistanceMeasureManager.h"
#include <vtkRenderer.h>
#include <vtkImageViewer2.h>
#include <QDebug>
#include <vtkCamera.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <vtkSphereSource.h>
#include <vtkRenderWindow.h>
#include <vtkMath.h>
#include <vtkAppendPolyData.h>
#include <vtkPropPicker.h>
#include <vtkCoordinate.h>
#include <vtkImageData.h>

SimpleDistanceMeasureManager::SimpleDistanceMeasureManager() = default;

SimpleDistanceMeasureManager::~SimpleDistanceMeasureManager() {
    Shutdown();
}

void SimpleDistanceMeasureManager::Initialize(vtkRenderer* overlayRenderer) {
    if (m_initialized) return;
    if (!overlayRenderer) return;
    m_overlayRenderer = overlayRenderer;
    m_initialized = true;
    qDebug() << "[SimpleDistanceMeasureManager] Initialized";
}

void SimpleDistanceMeasureManager::StartMeasure(const std::array<double, 3>& startPoint) {
    qDebug() << "[SimpleDistanceMeasureManager] StartMeasure - Point:" << startPoint[0] << startPoint[1] << startPoint[2];
}

void SimpleDistanceMeasureManager::UpdateMeasure(const std::array<double, 3>& endPoint) {
    qDebug() << "[SimpleDistanceMeasureManager] UpdateMeasure - Point:" << endPoint[0] << endPoint[1] << endPoint[2];
}

void SimpleDistanceMeasureManager::EndMeasure() {
    qDebug() << "[SimpleDistanceMeasureManager] EndMeasure";
}

void SimpleDistanceMeasureManager::SetVisible(bool visible) {
    m_visible = visible;
    qDebug() << "[SimpleDistanceMeasureManager] SetVisible:" << visible;
}

void SimpleDistanceMeasureManager::Shutdown() {
    if (!m_initialized) return;
    qDebug() << "[SimpleDistanceMeasureManager] Shutdown";
    m_overlayRenderer = nullptr;
    m_initialized = false;
}

void SimpleDistanceMeasureManager::SetColor(double r, double g, double b)
{
}

void SimpleDistanceMeasureManager::DrawStartPoint(std::array<double, 3> worldPoint) {
    if (!m_overlayRenderer) return;
    int id = generateNextId();
    auto [it, inserted] = m_measurements.emplace(id, Measurement{});
    assert(inserted);
    Measurement& m = it->second;
    m.id = id;
    m.startPointWorld = worldPoint;
    m.isComplete = false;

    m.startPointActor = createSphereActor(worldPoint);
    m.startCrosshairActor = createCrosshairActor(worldPoint, 5.0);

    m_overlayRenderer->AddActor(m.startPointActor);
    m_overlayRenderer->AddActor(m.startCrosshairActor);
}

void SimpleDistanceMeasureManager::DrawFinalMeasurementLine(std::array<double, 3> startPos, std::array<double, 3> endPos) {
    if (!m_overlayRenderer) return;
    ClearPreview();

    auto it = m_measurements.find(m_nextId);
    if (it == m_measurements.end()) return;
    Measurement& m = it->second;

    m.endPointWorld = endPos;
    m.endPointActor = createSphereActor(endPos);
    m.endCrosshairActor = createCrosshairActor(endPos, 5.0);
    m.isComplete = true;

    // 创建正式测量的固定线（不可更新）
    m.lineActor = createLineActor(startPos, endPos);
    m.tickActor = createTickActor(startPos, endPos);
    auto cam = m_overlayRenderer->GetActiveCamera();
    m.distanceLabel = createDistanceLabel(startPos, endPos, cam);

    m_overlayRenderer->AddActor(m.endPointActor);
    m_overlayRenderer->AddActor(m.endCrosshairActor);
    m_overlayRenderer->AddViewProp(m.lineActor);
    m_overlayRenderer->AddViewProp(m.tickActor);
    m_overlayRenderer->AddViewProp(m.distanceLabel);
}

void SimpleDistanceMeasureManager::DrawFinalMeasurementLine(int measurementId, std::array<double, 3> startPos, std::array<double, 3> endPos)
{
    if (!m_overlayRenderer) return;
    ClearPreview();

    auto it = m_measurements.find(measurementId);
    if (it == m_measurements.end()) return;

    Measurement& m = it->second;
    m.endPointWorld = endPos;
    m.startPointWorld = startPos; // 确保一致
    m.isComplete = true;

    // 创建正式测量的固定线（不可更新）
    m.lineActor = createLineActor(startPos, endPos);
    m.tickActor = createTickActor(startPos, endPos);
    auto cam = m_overlayRenderer->GetActiveCamera();
    m.distanceLabel = createDistanceLabel(startPos, endPos, cam);

    m.startPointActor = createSphereActor(startPos);
    m.startCrosshairActor = createCrosshairActor(startPos, 5.0);
    m.endPointActor = createSphereActor(endPos);
    m.endCrosshairActor = createCrosshairActor(endPos, 5.0);

    m_overlayRenderer->AddActor(m.startPointActor);
    m_overlayRenderer->AddActor(m.endPointActor);
    m_overlayRenderer->AddActor(m.startCrosshairActor);
    m_overlayRenderer->AddActor(m.endCrosshairActor);
    m_overlayRenderer->AddViewProp(m.lineActor);
    m_overlayRenderer->AddViewProp(m.tickActor);
    m_overlayRenderer->AddViewProp(m.distanceLabel);
}

void SimpleDistanceMeasureManager::PreviewMeasurementLine(
    std::array<double, 3> startPos,
    std::array<double, 3> currentPos)
{
    if (!m_overlayRenderer || !m_initialized) return;
    auto cam = m_overlayRenderer->GetActiveCamera();
    if (!cam) return;

    // === 懒初始化：只创建一次 ===
    if (!m_previewLineActor) {
        m_previewLineSource = vtkSmartPointer<vtkLineSource>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(m_previewLineSource->GetOutputPort());
        m_previewLineActor = vtkSmartPointer<vtkActor>::New();
        m_previewLineActor->SetMapper(mapper);
        m_previewLineActor->GetProperty()->SetColor(0.0, 1.0, 0.0);
        m_overlayRenderer->AddViewProp(m_previewLineActor);
    }

    if (!m_previewTickActor) {
        m_previewTickSource = vtkSmartPointer<vtkLineSource>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(m_previewTickSource->GetOutputPort());
        m_previewTickActor = vtkSmartPointer<vtkActor>::New();
        m_previewTickActor->SetMapper(mapper);
        m_previewTickActor->GetProperty()->SetColor(1.0, 1.0, 1.0);
        m_previewTickActor->GetProperty()->SetLineWidth(4.0);
        m_overlayRenderer->AddViewProp(m_previewTickActor);
    }

    if (!m_previewLabelActor) {
        m_previewTextSource = vtkSmartPointer<vtkVectorText>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(m_previewTextSource->GetOutputPort());
        m_previewLabelActor = vtkSmartPointer<vtkFollower>::New();
        m_previewLabelActor->SetMapper(mapper);
        m_previewLabelActor->GetProperty()->SetColor(1.0, 1.0, 1.0);
        m_previewLabelActor->SetScale(8.0, 8.0, 8.0);
        m_previewLabelActor->SetCamera(cam);
        m_overlayRenderer->AddViewProp(m_previewLabelActor);
    }

    // === 更新数据（零重建）===
    m_previewLineSource->SetPoint1(startPos.data());
    m_previewLineSource->SetPoint2(currentPos.data());
    m_previewLineSource->Modified();

    updatePreviewTick(startPos, currentPos, 3.0);
    updatePreviewLabel(startPos, currentPos, cam);
}

void SimpleDistanceMeasureManager::ClearPreview()
{
    if (!m_overlayRenderer) return;

    if (m_previewLineActor)      m_overlayRenderer->RemoveViewProp(m_previewLineActor);
    if (m_previewTickActor)      m_overlayRenderer->RemoveViewProp(m_previewTickActor);
    if (m_previewLabelActor)     m_overlayRenderer->RemoveViewProp(m_previewLabelActor);

    // 解除引用（触发释放）
    m_previewLineSource = nullptr;
    m_previewLineActor = nullptr;
    m_previewTickSource = nullptr;
    m_previewTickActor = nullptr;
    m_previewTextSource = nullptr;
    m_previewLabelActor = nullptr;
}

EditablePoint SimpleDistanceMeasureManager::GetEditablePoint(int screenX, int screenY) const {
    // ===== 第一步：尝试精确拾取（保留原有逻辑，快速路径） =====
    vtkNew<vtkPropPicker> picker;
    if (picker->PickProp(screenX, screenY, m_overlayRenderer)) {
        for (const auto& [id, m] : m_measurements) {
            if (!m.isComplete) continue;
            if (m.startPointActor == picker->GetViewProp()) {
                return { id, true };
            }
            if (m.endPointActor == picker->GetViewProp()) {
                return { id, false };
            }
        }
    }

    // ===== 第二步：容差拾取（fallback） =====
    constexpr double TOLERANCE_PX = 6.0; // 可根据 DPI 调整
    double minDist2 = TOLERANCE_PX * TOLERANCE_PX + 1.0; // 初始值 > 容差²
    EditablePoint bestMatch{ -1, false };

    // 创建坐标转换器（复用，避免频繁 new）
    vtkNew<vtkCoordinate> coord;
    coord->SetCoordinateSystemToWorld();
    coord->SetViewport(m_overlayRenderer);

    for (const auto& [id, m] : m_measurements) {
        if (!m.isComplete) continue;

        // 检查起点
        if (m.startPointActor && m.startPointActor->GetVisibility()) {
            coord->SetValue(m.startPointWorld.data());
            int* dispPos = coord->GetComputedDisplayValue(m_overlayRenderer);
            double dx = dispPos[0] - static_cast<double>(screenX);
            double dy = dispPos[1] - static_cast<double>(screenY);
            double dist2 = dx * dx + dy * dy;
            if (dist2 < minDist2) {
                minDist2 = dist2;
                bestMatch = { id, true };
            }
        }

        // 检查终点
        if (m.endPointActor && m.endPointActor->GetVisibility()) {
            coord->SetValue(m.endPointWorld.data());
            int* dispPos = coord->GetComputedDisplayValue(m_overlayRenderer);
            double dx = dispPos[0] - static_cast<double>(screenX);
            double dy = dispPos[1] - static_cast<double>(screenY);
            double dist2 = dx * dx + dy * dy;
            if (dist2 < minDist2) {
                minDist2 = dist2;
                bestMatch = { id, false };
            }
        }
    }

    // 如果最近点在容差范围内，返回它
    if (minDist2 <= TOLERANCE_PX * TOLERANCE_PX) {
        return bestMatch;
    }

    return {}; // 未命中
}

void SimpleDistanceMeasureManager::UpdateMeasurementPoint(
    int measurementId, bool isStart, const std::array<double, 3>& newWorldPos) {

    auto it = m_measurements.find(measurementId);
    if (it == m_measurements.end() || !it->second.isComplete) return;

    Measurement& m = it->second;
    std::array<double, 3> other = isStart ? m.endPointWorld : m.startPointWorld;
    std::array<double, 3> newStart = isStart ? newWorldPos : other;
    std::array<double, 3> newEnd = isStart ? other : newWorldPos;

    // 移除旧的
    m_overlayRenderer->RemoveViewProp(m.startPointActor);
    m_overlayRenderer->RemoveViewProp(m.endPointActor);
    m_overlayRenderer->RemoveViewProp(m.startCrosshairActor);
    m_overlayRenderer->RemoveViewProp(m.endCrosshairActor);
    m_overlayRenderer->RemoveViewProp(m.lineActor);
    m_overlayRenderer->RemoveViewProp(m.tickActor);
    m_overlayRenderer->RemoveViewProp(m.distanceLabel);

    // 重绘新的
    DrawFinalMeasurementLine(measurementId, newStart, newEnd);
}

void SimpleDistanceMeasureManager::ClearAllMeasurement() {
    if (!m_overlayRenderer) return;

    for (auto& pair : m_measurements) {
        Measurement& m = pair.second;
        m_overlayRenderer->RemoveViewProp(m.startPointActor);
        m_overlayRenderer->RemoveViewProp(m.endPointActor);
        m_overlayRenderer->RemoveViewProp(m.startCrosshairActor);
        m_overlayRenderer->RemoveViewProp(m.endCrosshairActor);
        m_overlayRenderer->RemoveViewProp(m.lineActor);
        m_overlayRenderer->RemoveViewProp(m.tickActor);
        m_overlayRenderer->RemoveViewProp(m.distanceLabel);
    }
    m_measurements.clear();
    m_nextId = 0;

    ClearPreview();
}

void SimpleDistanceMeasureManager::ClearCurrentMeasurement() {

    ClearPreview();
    // m_nextId 即最近一次 generateNextId 的值 = 最新一条测量的 id
    auto it = m_measurements.find(m_nextId);
    if (it == m_measurements.end()) return;
    Measurement& m = it->second;
    m_overlayRenderer->RemoveViewProp(m.startPointActor);
    m_overlayRenderer->RemoveViewProp(m.startCrosshairActor);
    m_overlayRenderer->RemoveViewProp(m.lineActor);
    m_overlayRenderer->RemoveViewProp(m.tickActor);
    m_overlayRenderer->RemoveViewProp(m.distanceLabel);
    // 必须从 map 移除，否则残留悬空条目，后续 find/遍历会拿到已移除的 actor
    m_measurements.erase(it);
}

std::vector<MeasurementRecord> SimpleDistanceMeasureManager::GetMeasurementRecords() const
{
    std::vector<MeasurementRecord> out;
    out.reserve(m_measurements.size());
    for (const auto &kv : m_measurements) {
        const Measurement &m = kv.second;
        MeasurementRecord r;
        r.kind = MeasurementRecord::Kind::Distance;
        r.id = m.id;
        r.complete = m.isComplete;
        r.pointCount = 2;
        r.points[0] = m.startPointWorld;
        r.points[1] = m.endPointWorld;
        const double dx = m.endPointWorld[0] - m.startPointWorld[0];
        const double dy = m.endPointWorld[1] - m.startPointWorld[1];
        const double dz = m.endPointWorld[2] - m.startPointWorld[2];
        r.value = std::sqrt(dx * dx + dy * dy + dz * dz);
        out.push_back(r);
    }
    return out;
}

// ================== 工厂函数（保持不变）==================
vtkSmartPointer<vtkActor> SimpleDistanceMeasureManager::createSphereActor(const std::array<double, 3>& point) {
    vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter(point.data());
    sphereSource->SetRadius(0.5);
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphereSource->GetOutputPort());
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 0.0, 0.0);
    return actor;
}

vtkSmartPointer<vtkActor> SimpleDistanceMeasureManager::createLineActor(const std::array<double, 3>& startPoint, const std::array<double, 3>& endPoint) {
    vtkSmartPointer<vtkLineSource> lineSource = vtkSmartPointer<vtkLineSource>::New();
    lineSource->SetPoint1(startPoint.data());
    lineSource->SetPoint2(endPoint.data());
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(lineSource->GetOutputPort());
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 1.0, 0.0);
    return actor;
}

vtkSmartPointer<vtkActor> SimpleDistanceMeasureManager::createCrosshairActor(const std::array<double, 3>& center, double length) {
    auto xLine = vtkSmartPointer<vtkLineSource>::New();
    xLine->SetPoint1(center[0] - length, center[1], center[2]);
    xLine->SetPoint2(center[0] + length, center[1], center[2]);
    auto yLine = vtkSmartPointer<vtkLineSource>::New();
    yLine->SetPoint1(center[0], center[1] - length, center[2]);
    yLine->SetPoint2(center[0], center[1] + length, center[2]);
    auto zLine = vtkSmartPointer<vtkLineSource>::New();
    zLine->SetPoint1(center[0], center[1], center[2] - length);
    zLine->SetPoint2(center[0], center[1], center[2] + length);

    auto append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputConnection(xLine->GetOutputPort());
    append->AddInputConnection(yLine->GetOutputPort());
    append->AddInputConnection(zLine->GetOutputPort());

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(append->GetOutputPort());
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 1.0, 1.0);
    actor->GetProperty()->SetLineWidth(2.0);
    return actor;
}

vtkSmartPointer<vtkActor> SimpleDistanceMeasureManager::createTickActor(
    const std::array<double, 3>& p1,
    const std::array<double, 3>& p2,
    double tickLength)
{
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-6) return nullptr;

    double mid[3] = { (p1[0] + p2[0]) / 2.0, (p1[1] + p2[1]) / 2.0, (p1[2] + p2[2]) / 2.0 };
    //double dir[3] = { dx / len, dy / len, dz / len };

    double dir[3] = { dx / len, dy / len, dz / len };
    double ref[3];
    if (std::abs(dir[2]) ==0) {
        ref[0] = 0.0; ref[1] = 0.0; ref[2] = 1.0;
    }
    else if (std::abs(dir[1]) ==0) {
        ref[0] = 0.0; ref[1] = 1.0; ref[2] = 0.0;
    }
    else
    {
        ref[0] = 1.0; ref[1] = 0.0; ref[2] = 0.0;
    }

    double perp[3];
    vtkMath::Cross(dir, ref, perp);
    double perpLen = vtkMath::Norm(perp);
    if (perpLen < 1e-6) {
        ref[0] = 0.0; ref[1] = 1.0; ref[2] = 0.0;
        vtkMath::Cross(dir, ref, perp);
        perpLen = vtkMath::Norm(perp);
        if (perpLen < 1e-6) return nullptr;
    }
    vtkMath::Normalize(perp);

    double start[3] = {
        mid[0] - (tickLength / 2.0) * perp[0],
        mid[1] - (tickLength / 2.0) * perp[1],
        mid[2] - (tickLength / 2.0) * perp[2]
    };
    double end[3] = {
        mid[0] + (tickLength / 2.0) * perp[0],
        mid[1] + (tickLength / 2.0) * perp[1],
        mid[2] + (tickLength / 2.0) * perp[2]
    };

    auto source = vtkSmartPointer<vtkLineSource>::New();
    source->SetPoint1(start);
    source->SetPoint2(end);
    source->Update();

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(source->GetOutputPort());
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 1.0, 1.0);
    actor->GetProperty()->SetLineWidth(4.0);
    return actor;
}

vtkSmartPointer<vtkFollower> SimpleDistanceMeasureManager::createDistanceLabel(
    const std::array<double, 3>& p1,
    const std::array<double, 3>& p2,
    vtkCamera* camera,
    double scale,
    double offset)
{
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    qDebug() << "[SimpleDistanceMeasureManager] createDistanceLabel - Distance:" << distance;

    double mid[3] = { (p1[0] + p2[0]) / 2.0, (p1[1] + p2[1]) / 2.0, (p1[2] + p2[2]) / 2.0 };

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << distance << " mm";
    auto textSource = vtkSmartPointer<vtkVectorText>::New();
    textSource->SetText(oss.str().c_str());

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(textSource->GetOutputPort());

    auto follower = vtkSmartPointer<vtkFollower>::New();
    follower->SetMapper(mapper);
    follower->GetProperty()->SetColor(1.0, 1.0, 1.0);
    follower->SetScale(scale, scale, scale);
    follower->SetPosition(mid);

    if (camera) {
        follower->SetCamera(camera);
    }

    return follower;
}

// ================== 新增：预览更新辅助函数 ==================
void SimpleDistanceMeasureManager::updatePreviewTick(
    const std::array<double, 3>& p1,
    const std::array<double, 3>& p2,
    double tickLength)
{
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    double len = std::sqrt(dx * dx + dy * dy + dz * dz);

    double mid[3] = { (p1[0] + p2[0]) / 2.0, (p1[1] + p2[1]) / 2.0, (p1[2] + p2[2]) / 2.0 };

    if (len < 1e-6) {
        m_previewTickSource->SetPoint1(mid);
        m_previewTickSource->SetPoint2(mid);
    }
    else {
        double dir[3] = { dx / len, dy / len, dz / len };
        double ref[3];
        if (std::abs(dir[2]) ==0) {
            ref[0] = 0.0; ref[1] = 0.0; ref[2] = 1.0;
        }
        else if (std::abs(dir[1])==0){
            ref[0] = 0.0; ref[1] = 1.0; ref[2] = 0.0;
        }
        else
        {
            ref[0] = 1.0; ref[1] = 0.0; ref[2] = 0.0;
        }

        double perp[3];
        vtkMath::Cross(dir, ref, perp);
        double perpLen = vtkMath::Norm(perp);
        if (perpLen < 1e-6) {
            ref[0] = 0.0; ref[1] = 1.0; ref[2] = 0.0;
            vtkMath::Cross(dir, ref, perp);
            perpLen = vtkMath::Norm(perp);
        }
        if (perpLen > 1e-6) {
            vtkMath::Normalize(perp);
        }

		qDebug() << "perp:" << perp[0] << perp[1] << perp[2];

        double start[3] = {
            mid[0] - (tickLength / 2.0) * perp[0],
            mid[1] - (tickLength / 2.0) * perp[1],
            mid[2] - (tickLength / 2.0) * perp[2]
        };
        double end[3] = {
            mid[0] + (tickLength / 2.0) * perp[0],
            mid[1] + (tickLength / 2.0) * perp[1],
            mid[2] + (tickLength / 2.0) * perp[2]
        };

		qDebug() << "tick start:" << start[0] << start[1] << start[2];
		qDebug() << "tick end:" << end[0] << end[1] << end[2];
        m_previewTickSource->SetPoint1(start);
        m_previewTickSource->SetPoint2(end);
    }
    m_previewTickSource->Modified();
}

void SimpleDistanceMeasureManager::updatePreviewLabel(
    const std::array<double, 3>& p1,
    const std::array<double, 3>& p2,
    vtkCamera* camera)
{
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << distance << " mm";
    m_previewTextSource->SetText(oss.str().c_str());
    m_previewTextSource->Modified();

    double mid[3] = { (p1[0] + p2[0]) / 2.0, (p1[1] + p2[1]) / 2.0, (p1[2] + p2[2]) / 2.0 };
    m_previewLabelActor->SetPosition(mid);
}

//bool SimpleDistanceMeasureManager::IsWorldPointInImage(const std::array<double, 3>& worldPoint) const
//{
//    if (!m_hasImageBounds) {
//        return true; // 无边界信息时保守允许
//    }
//
//    const double eps = 1e-3; // 容差
//    const auto& b = m_imageWorldBounds;
//    const auto& p = worldPoint;
//
//    return (p[0] >= b[0] - eps && p[0] <= b[1] + eps &&
//        p[1] >= b[2] - eps && p[1] <= b[3] + eps &&
//        p[2] >= b[4] - eps && p[2] <= b[5] + eps);
//}

void SimpleDistanceMeasureManager::OnSliceChanged(vtkImageViewer2* viewer,
    int slice,
    ViewType viewType)
{
    auto nonConstViewer = const_cast<vtkImageViewer2*>(viewer);
    vtkImageData* image = nonConstViewer->GetInput();
    if (!m_initialized || !viewer || !nonConstViewer->GetInput()) return;

    // 获取图像的 spacing 和 origin
    double spacing[3];
    double origin[3];
    nonConstViewer->GetInput()->GetSpacing(spacing);
    nonConstViewer->GetInput()->GetOrigin(origin);

    for (auto& kv : m_measurements) {
        Measurement& m = kv.second;
        if (!m.isComplete) {
            continue; // 跳过未完成的测量
        }

        // 更新 m_lastWorldPoint 对应的分量
        switch (viewType) {
        case ViewType::Axial:    // Z 轴方向
            m.startPointWorld[2] = origin[2] + slice * spacing[2];
            m.endPointWorld[2] = origin[2] + slice * spacing[2];
            break;
        case ViewType::Sagittal: // X 轴方向
            m.startPointWorld[0] = origin[0] + slice * spacing[0];
            m.endPointWorld[0] = origin[0] + slice * spacing[0];
            break;
        case ViewType::Coronal:  // Y 轴方向
            m.startPointWorld[1] = origin[1] + slice * spacing[1];
            m.endPointWorld[1] = origin[1] + slice * spacing[1];
            break;
        default:
            return;
        }

        UpdateMeasurementPoint(m.id, true, m.startPointWorld);
        UpdateMeasurementPoint(m.id, false, m.endPointWorld);
    }
}
