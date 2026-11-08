#include "SimpleAngleMeasureManager.h"
#include <vtkRenderer.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkLine.h>
#include <vtkPolyLine.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageViewer2.h>
#include <vtkImageData.h>
#include <QDebug>
#include <vtkLineSource.h>
#include <vtkAppendPolyData.h>
#include <vtkSphereSource.h>
#include <vtkMath.h>
#include <vtkPropPicker.h>
#include <vtkVectorText.h>
#include <vtkCoordinate.h>
#include <vtkFollower.h>

// ============================================================
//  生命周期
// ============================================================

SimpleAngleMeasureManager::SimpleAngleMeasureManager() = default;

SimpleAngleMeasureManager::~SimpleAngleMeasureManager() {
    Shutdown();
}

void SimpleAngleMeasureManager::Initialize(vtkRenderer* overlayRenderer) {
    if (m_initialized) return;
    if (!overlayRenderer) return;

    m_overlayRenderer = overlayRenderer;
    m_initialized = true;

    qDebug() << "[SimpleAngleMeasureManager] Initialized";
}

void SimpleAngleMeasureManager::SetColor(double r, double g, double b) {
    // 暂未实现颜色统一设置（第8条优化保留）
}

void SimpleAngleMeasureManager::SetVisible(bool visible) {
    m_visible = visible;
    qDebug() << "[SimpleAngleMeasureManager] SetVisible:" << visible;
}

void SimpleAngleMeasureManager::Shutdown() {
    if (!m_initialized) return;

    qDebug() << "[SimpleAngleMeasureManager] Shutdown";
    m_overlayRenderer = nullptr;
    m_viewer = nullptr;
    m_initialized = false;
}

// ============================================================
//  接口方法 —— 委托给内部 Draw* 系列
// ============================================================

void SimpleAngleMeasureManager::StartMeasure(const std::array<double, 3>& point1) {
    DrawStartPoint(point1);
}

void SimpleAngleMeasureManager::UpdateMeasure(const std::array<double, 3>& point2) {
    // 由外部在鼠标移动时调用，更新预览线
    // 实际的 start→middle 预览线由调用方传入当前起点+当前鼠标位置
    // 此处仅作日志，具体预览由 PreviewStartToMiddleMeasurementLine 驱动
    qDebug() << "[SimpleAngleMeasureManager] UpdateMeasure - Point2:"
        << point2[0] << point2[1] << point2[2];
}

void SimpleAngleMeasureManager::EndMeasure(const std::array<double, 3>& point3) {
    DrawEndPointAndMiddleToEndLine(point3);
}

// ============================================================
//  绘制流程
// ============================================================

void SimpleAngleMeasureManager::DrawStartPoint(const std::array<double, 3>& worldPoint) {
    if (!m_overlayRenderer) return;

    // 生成新 ID 并同步到 m_currentId
    int id = generateNextId();
    m_currentId = id;  // 修复：确保后续 find(m_currentId) 能找到本次测量

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

void SimpleAngleMeasureManager::DrawMiddlePointAndStartToMiddleLine(const std::array<double, 3>& worldPoint) {
    if (!m_overlayRenderer) return;

    // 修复：检查 find 结果，防止 UB
    auto it = m_measurements.find(m_currentId);
    if (it == m_measurements.end()) return;
    Measurement& m = it->second;

    m.middlePointWorld = worldPoint;

    m.startToMiddleLineActor = createLineActor(m.startPointWorld, m.middlePointWorld);
    m.middlePointActor = createSphereActor(worldPoint);
    m.middleCrosshairActor = createCrosshairActor(worldPoint, 5.0);

    m_overlayRenderer->AddActor(m.startToMiddleLineActor);
    m_overlayRenderer->AddActor(m.middlePointActor);
    m_overlayRenderer->AddActor(m.middleCrosshairActor);
}

void SimpleAngleMeasureManager::DrawEndPointAndMiddleToEndLine(const std::array<double, 3>& worldPoint) {
    if (!m_overlayRenderer) return;

    auto it = m_measurements.find(m_currentId);
    if (it == m_measurements.end()) return;
    Measurement& m = it->second;

    m.endPointWorld = worldPoint;
    m.isComplete = true;

    m.middleToEndLineActor = createLineActor(m.middlePointWorld, worldPoint);
    m.endPointActor = createSphereActor(worldPoint);
    m.endCrosshairActor = createCrosshairActor(worldPoint, 5.0);

    m_overlayRenderer->AddActor(m.middleToEndLineActor);
    m_overlayRenderer->AddActor(m.endPointActor);
    m_overlayRenderer->AddActor(m.endCrosshairActor);

    // 计算角度
    double angleDeg = ComputeAngle(m.startPointWorld, m.middlePointWorld, m.endPointWorld);

    // 计算标签位置（角内部）
    std::array<double, 3> labelPos = ComputeAngleLabelPosition(
        m.startPointWorld, m.middlePointWorld, m.endPointWorld, 0.3);

    auto cam = m_overlayRenderer->GetActiveCamera();
    if (!cam) return;

    m.angleArcActor = CreateAngleArc(m.startPointWorld, m.middlePointWorld, m.endPointWorld);
    m.angleLabel = CreateAngleLabel(angleDeg, labelPos, cam);

    if (m.angleArcActor) m_overlayRenderer->AddActor(m.angleArcActor);
    if (m.angleLabel)    m_overlayRenderer->AddViewProp(m.angleLabel);

    qDebug() << "[Angle] Calculated:" << QString::number(angleDeg, 'f', 1) << "deg";

    ClearPreview();
}

// ============================================================
//  预览线段（懒初始化，零重建）
// ============================================================

void SimpleAngleMeasureManager::PreviewStartToMiddleMeasurementLine(
    const std::array<double, 3>& startPos,
    const std::array<double, 3>& currentPos)
{
    if (!m_overlayRenderer || !m_initialized) return;

    // 懒初始化：仅在第一次调用时创建 Actor
    if (!m_previewStartToMiddleLineActor) {
        m_previewStartToMiddleLineSource = vtkSmartPointer<vtkLineSource>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(m_previewStartToMiddleLineSource->GetOutputPort());
        m_previewStartToMiddleLineActor = vtkSmartPointer<vtkActor>::New();
        m_previewStartToMiddleLineActor->SetMapper(mapper);
        m_previewStartToMiddleLineActor->GetProperty()->SetColor(0.0, 1.0, 0.0);
        m_overlayRenderer->AddViewProp(m_previewStartToMiddleLineActor);
    }

    // 直接更新端点，不重建 Actor
    m_previewStartToMiddleLineSource->SetPoint1(startPos.data());
    m_previewStartToMiddleLineSource->SetPoint2(currentPos.data());
    m_previewStartToMiddleLineSource->Modified();
}

void SimpleAngleMeasureManager::PreviewMiddleToEndMeasurementLine(
    const std::array<double, 3>& startPos,
    const std::array<double, 3>& currentPos)
{
    if (!m_overlayRenderer || !m_initialized) return;
    auto cam = m_overlayRenderer->GetActiveCamera();
    if (!cam) return;

    // 懒初始化预览线段（middle → end）
    if (!m_previewMiddleToEndLineActor) {
        m_previewMiddleToEndLineSource = vtkSmartPointer<vtkLineSource>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(m_previewMiddleToEndLineSource->GetOutputPort());
        m_previewMiddleToEndLineActor = vtkSmartPointer<vtkActor>::New();
        m_previewMiddleToEndLineActor->SetMapper(mapper);
        m_previewMiddleToEndLineActor->GetProperty()->SetColor(0.0, 1.0, 0.0);
        m_overlayRenderer->AddViewProp(m_previewMiddleToEndLineActor);
    }
    m_previewMiddleToEndLineSource->SetPoint1(startPos.data());
    m_previewMiddleToEndLineSource->SetPoint2(currentPos.data());
    m_previewMiddleToEndLineSource->Modified();

    // 更新预览弧线和标签
    auto it = m_measurements.find(m_currentId);
    if (it != m_measurements.end()) {
        const Measurement& m = it->second;
        double angleDeg = ComputeAngle(m.startPointWorld, startPos, currentPos);
        UpdatePreviewAngleArc(m.startPointWorld, startPos, currentPos);
        UpdatePreviewAngleLabel(angleDeg, m.startPointWorld, startPos, currentPos, cam);
    }
}

// ============================================================
//  预览弧线更新（懒初始化，零重建）
// ============================================================

void SimpleAngleMeasureManager::UpdatePreviewAngleArc(
    const std::array<double, 3>& start,
    const std::array<double, 3>& vertex,
    const std::array<double, 3>& end)
{
    // 懒初始化：创建空 PolyData 容器，后续只更新点
    if (!m_previewAngleArcActor) {
        m_previewArcPoints = vtkSmartPointer<vtkPoints>::New();
        m_previewArcPolyLine = vtkSmartPointer<vtkPolyLine>::New();
        m_previewArcCells = vtkSmartPointer<vtkCellArray>::New();
        m_previewArcPolyData = vtkSmartPointer<vtkPolyData>::New();

        m_previewArcPolyLine->GetPointIds()->SetNumberOfIds(0);
        m_previewArcCells->InsertNextCell(m_previewArcPolyLine);
        m_previewArcPolyData->SetPoints(m_previewArcPoints);
        m_previewArcPolyData->SetLines(m_previewArcCells);

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(m_previewArcPolyData);

        m_previewAngleArcActor = vtkSmartPointer<vtkActor>::New();
        m_previewAngleArcActor->SetMapper(mapper);
        m_previewAngleArcActor->GetProperty()->SetColor(1.0, 0.5, 0.0);
        m_previewAngleArcActor->GetProperty()->SetLineWidth(2);
        m_overlayRenderer->AddActor(m_previewAngleArcActor);
    }

    // 复用 GenerateArcPoints 更新弧线点数据
    GenerateArcPoints(start, vertex, end, m_previewArcPoints, m_previewArcPolyLine);

    // 复用 cells（原先每次移动都 New 一个，测量拖动时分配抖动明显）
    if (!m_previewArcCells) {
        m_previewArcCells = vtkSmartPointer<vtkCellArray>::New();
    }
    m_previewArcCells->Reset();
    if (m_previewArcPoints->GetNumberOfPoints() > 1) {
        m_previewArcCells->InsertNextCell(m_previewArcPolyLine);
    }

    m_previewArcPolyData->SetPoints(m_previewArcPoints);
    m_previewArcPolyData->SetLines(m_previewArcCells);
    m_previewArcPolyData->Modified();
}

// ============================================================
//  预览标签更新（懒初始化，零重建）
// ============================================================

void SimpleAngleMeasureManager::UpdatePreviewAngleLabel(
    double angleDeg,
    const std::array<double, 3>& start,
    const std::array<double, 3>& vertex,
    const std::array<double, 3>& end,
    vtkCamera* camera)
{
    // 懒初始化
    if (!m_previewAngleLabel) {
        m_previewLabelText = vtkSmartPointer<vtkVectorText>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(m_previewLabelText->GetOutputPort());

        m_previewAngleLabel = vtkSmartPointer<vtkFollower>::New();
        m_previewAngleLabel->SetMapper(mapper);
        m_previewAngleLabel->SetScale(8.0, 8.0, 8.0);
        m_previewAngleLabel->SetCamera(camera);
        m_previewAngleLabel->GetProperty()->SetColor(1.0, 1.0, 0.0);
        m_overlayRenderer->AddViewProp(m_previewAngleLabel);
    }

    // 直接更新文字内容（不重建 Follower）
    QString text = QString::number(angleDeg, 'f', 1) + "deg";
    m_previewLabelText->SetText(text.toStdString().c_str());
    m_previewLabelText->Modified();

    // 更新标签位置
    std::array<double, 3> labelPos = ComputeAngleLabelPosition(start, vertex, end, 0.3);
    m_previewAngleLabel->SetPosition(labelPos[0], labelPos[1], labelPos[2]);
}

// ============================================================
//  最终测量重建（编辑点后全量重绘）
// ============================================================

void SimpleAngleMeasureManager::DrawFinalAngleMeasurement(int measurementId) {
    auto it = m_measurements.find(measurementId);
    if (it == m_measurements.end()) return;
    Measurement& m = it->second;

    // 创建点和十字线 Actor
    m.startPointActor = createSphereActor(m.startPointWorld);
    m.middlePointActor = createSphereActor(m.middlePointWorld);
    m.endPointActor = createSphereActor(m.endPointWorld);
    m.startCrosshairActor = createCrosshairActor(m.startPointWorld, 5.0);
    m.middleCrosshairActor = createCrosshairActor(m.middlePointWorld, 5.0);
    m.endCrosshairActor = createCrosshairActor(m.endPointWorld, 5.0);

    // 创建连接线 Actor
    m.startToMiddleLineActor = createLineActor(m.startPointWorld, m.middlePointWorld);
    m.middleToEndLineActor = createLineActor(m.middlePointWorld, m.endPointWorld);

    // 计算角度和标签
    double angleDeg = ComputeAngle(m.startPointWorld, m.middlePointWorld, m.endPointWorld);
    std::array<double, 3> labelPos = ComputeAngleLabelPosition(
        m.startPointWorld, m.middlePointWorld, m.endPointWorld, 0.3);

    auto cam = m_overlayRenderer->GetActiveCamera();
    if (!cam) return;

    m.angleArcActor = CreateAngleArc(m.startPointWorld, m.middlePointWorld, m.endPointWorld);
    m.angleLabel = CreateAngleLabel(angleDeg, labelPos, cam);

    // 将所有 Actor 添加到渲染器
    m_overlayRenderer->AddActor(m.startPointActor);
    m_overlayRenderer->AddActor(m.middlePointActor);
    m_overlayRenderer->AddActor(m.endPointActor);
    m_overlayRenderer->AddActor(m.startCrosshairActor);
    m_overlayRenderer->AddActor(m.middleCrosshairActor);
    m_overlayRenderer->AddActor(m.endCrosshairActor);
    m_overlayRenderer->AddActor(m.startToMiddleLineActor);
    m_overlayRenderer->AddActor(m.middleToEndLineActor);
    if (m.angleArcActor) m_overlayRenderer->AddActor(m.angleArcActor);
    if (m.angleLabel)    m_overlayRenderer->AddViewProp(m.angleLabel);
}

// ============================================================
//  移除指定测量的所有 Actor（辅助函数，消除重复 lambda）
// ============================================================

void SimpleAngleMeasureManager::removeMeasurementActors(int measurementId) {
    auto it = m_measurements.find(measurementId);
    if (it == m_measurements.end()) return;
    Measurement& m = it->second;

    // 辅助 lambda：安全移除 Actor / ViewProp
    auto removeActor = [this](vtkSmartPointer<vtkProp> actor) {
        if (actor) m_overlayRenderer->RemoveActor(actor);
        };
    auto removeViewProp = [this](vtkSmartPointer<vtkProp> prop) {
        if (prop) m_overlayRenderer->RemoveViewProp(prop);
        };

    removeActor(m.startPointActor);
    removeActor(m.startCrosshairActor);
    removeActor(m.middlePointActor);
    removeActor(m.middleCrosshairActor);
    removeActor(m.endPointActor);
    removeActor(m.endCrosshairActor);
    removeActor(m.startToMiddleLineActor);
    removeActor(m.middleToEndLineActor);
    removeActor(m.angleArcActor);
    removeViewProp(m.angleLabel);
}

// ============================================================
//  清除
// ============================================================

void SimpleAngleMeasureManager::ClearPreview() {
    if (!m_overlayRenderer || !m_initialized) return;

    if (m_previewStartToMiddleLineActor) {
        m_overlayRenderer->RemoveViewProp(m_previewStartToMiddleLineActor);
        m_previewStartToMiddleLineActor = nullptr;
        m_previewStartToMiddleLineSource = nullptr;
    }
    if (m_previewMiddleToEndLineActor) {
        m_overlayRenderer->RemoveViewProp(m_previewMiddleToEndLineActor);
        m_previewMiddleToEndLineActor = nullptr;
        m_previewMiddleToEndLineSource = nullptr;
    }
    if (m_previewAngleArcActor) {
        m_overlayRenderer->RemoveActor(m_previewAngleArcActor);
        m_previewAngleArcActor = nullptr;
        m_previewArcPoints = nullptr;
        m_previewArcPolyLine = nullptr;
        m_previewArcCells = nullptr;
        m_previewArcPolyData = nullptr;
    }
    if (m_previewAngleLabel) {
        m_overlayRenderer->RemoveViewProp(m_previewAngleLabel);
        m_previewAngleLabel = nullptr;
        m_previewLabelText = nullptr;
    }
}

void SimpleAngleMeasureManager::ClearCurrentMeasurement() {
    if (!m_overlayRenderer || m_measurements.empty()) return;

    ClearPreview();  // 先清除预览元素

    auto it = m_measurements.find(m_currentId);
    if (it == m_measurements.end()) return;

    removeMeasurementActors(m_currentId);
    m_measurements.erase(it);
}

void SimpleAngleMeasureManager::ClearAllMeasurement() {
    if (!m_overlayRenderer || m_measurements.empty()) return;

    ClearPreview();  // 先清除预览元素

    for (auto& kv : m_measurements) {
        removeMeasurementActors(kv.first);
    }
    m_measurements.clear();
}

std::vector<MeasurementRecord> SimpleAngleMeasureManager::GetMeasurementRecords() const
{
    std::vector<MeasurementRecord> out;
    out.reserve(m_measurements.size());
    for (const auto &kv : m_measurements) {
        const Measurement &m = kv.second;
        MeasurementRecord r;
        r.kind = MeasurementRecord::Kind::Angle;
        r.id = m.id;
        r.complete = m.isComplete;
        r.pointCount = 3;
        r.points[0] = m.startPointWorld;
        r.points[1] = m.middlePointWorld;
        r.points[2] = m.endPointWorld;
        r.value = ComputeAngle(m.startPointWorld, m.middlePointWorld, m.endPointWorld);
        out.push_back(r);
    }
    return out;
}

// ============================================================
//  拾取与编辑
// ============================================================

EditableAnglePoint SimpleAngleMeasureManager::GetEditableAnglePoint(int screenX, int screenY) const {
    if (!m_overlayRenderer) return {};

    // 第一步：精确 PropPicker 拾取
    vtkNew<vtkPropPicker> picker;
    if (picker->PickProp(screenX, screenY, m_overlayRenderer)) {
        for (const auto& [id, m] : m_measurements) {
            if (!m.isComplete) continue;
            if (m.startPointActor == picker->GetViewProp()) return { id, AnglePointRole::Start };
            if (m.middlePointActor == picker->GetViewProp()) return { id, AnglePointRole::Middle };
            if (m.endPointActor == picker->GetViewProp()) return { id, AnglePointRole::End };
        }
    }

    // 第二步：容差拾取（fallback，像素距离 <= 6px）
    constexpr double TOLERANCE_PX = 6.0;
    double minDist2 = TOLERANCE_PX * TOLERANCE_PX + 1.0;
    EditableAnglePoint bestMatch{ -1, AnglePointRole::None };

    vtkNew<vtkCoordinate> coord;
    coord->SetCoordinateSystemToWorld();
    coord->SetViewport(m_overlayRenderer);

    // 对每个完成测量的三个控制点分别计算屏幕距离
    auto checkPoint = [&](int id, AnglePointRole role,
        vtkActor* actor, const std::array<double, 3>& worldPos) {
            if (!actor || !actor->GetVisibility()) return;
            coord->SetValue(worldPos.data());
            int* dispPos = coord->GetComputedDisplayValue(m_overlayRenderer);
            double dx = dispPos[0] - static_cast<double>(screenX);
            double dy = dispPos[1] - static_cast<double>(screenY);
            double dist2 = dx * dx + dy * dy;
            if (dist2 < minDist2) {
                minDist2 = dist2;
                bestMatch = { id, role };
            }
        };

    for (const auto& [id, m] : m_measurements) {
        if (!m.isComplete) continue;
        checkPoint(id, AnglePointRole::Start, m.startPointActor, m.startPointWorld);
        checkPoint(id, AnglePointRole::Middle, m.middlePointActor, m.middlePointWorld);
        checkPoint(id, AnglePointRole::End, m.endPointActor, m.endPointWorld);
    }

    return (minDist2 <= TOLERANCE_PX * TOLERANCE_PX) ? bestMatch : EditableAnglePoint{};
}

void SimpleAngleMeasureManager::UpdateAngleMeasurementPoint(
    int measurementId, AnglePointRole role, const std::array<double, 3>& newWorldPos)
{
    auto it = m_measurements.find(measurementId);
    if (it == m_measurements.end() || !it->second.isComplete) return;

    Measurement& m = it->second;

    // 更新对应控制点坐标
    switch (role) {
    case AnglePointRole::Start:  m.startPointWorld = newWorldPos; break;
    case AnglePointRole::Middle: m.middlePointWorld = newWorldPos; break;
    case AnglePointRole::End:    m.endPointWorld = newWorldPos; break;
    default: return;
    }

    // 移除旧 Actor，重建新 Actor
    removeMeasurementActors(measurementId);
    DrawFinalAngleMeasurement(measurementId);
}

// ============================================================
//  切片变更
// ============================================================

void SimpleAngleMeasureManager::OnSliceChanged(vtkImageViewer2* viewer, int slice, ViewType viewType) {
    if (!m_initialized || !viewer) return;

    // 避免 const_cast 后再次判断 null
    auto* nonConstViewer = const_cast<vtkImageViewer2*>(viewer);
    if (!nonConstViewer->GetInput()) return;

    double spacing[3];
    double origin[3];
    nonConstViewer->GetInput()->GetSpacing(spacing);
    nonConstViewer->GetInput()->GetOrigin(origin);

    for (auto& kv : m_measurements) {
        Measurement& m = kv.second;
        if (!m.isComplete) continue;

        // 将所有点的对应轴坐标同步到当前切片
        switch (viewType) {
        case ViewType::Axial:    // Z 轴
            m.startPointWorld[2] = origin[2] + slice * spacing[2];
            m.middlePointWorld[2] = origin[2] + slice * spacing[2];
            m.endPointWorld[2] = origin[2] + slice * spacing[2];
            break;
        case ViewType::Sagittal: // X 轴
            m.startPointWorld[0] = origin[0] + slice * spacing[0];
            m.middlePointWorld[0] = origin[0] + slice * spacing[0];
            m.endPointWorld[0] = origin[0] + slice * spacing[0];
            break;
        case ViewType::Coronal:  // Y 轴
            m.startPointWorld[1] = origin[1] + slice * spacing[1];
            m.middlePointWorld[1] = origin[1] + slice * spacing[1];
            m.endPointWorld[1] = origin[1] + slice * spacing[1];
            break;
        default:
            continue;
        }

        // 修复：坐标已在上方直接修改，这里只需重绘一次，不再三次调用 UpdateAngleMeasurementPoint
        removeMeasurementActors(m.id);
        DrawFinalAngleMeasurement(m.id);
    }
}

// ============================================================
//  静态计算工具
// ============================================================

double SimpleAngleMeasureManager::ComputeAngle(
    const std::array<double, 3>& startPoint,
    const std::array<double, 3>& vertexPoint,
    const std::array<double, 3>& endPoint)
{
    // 修复：两个向量均从顶点出发，保证夹角计算正确
    // v1: vertex -> start
    std::array<double, 3> v1 = {
        startPoint[0] - vertexPoint[0],
        startPoint[1] - vertexPoint[1],
        startPoint[2] - vertexPoint[2]
    };
    // v2: vertex -> end
    std::array<double, 3> v2 = {
        endPoint[0] - vertexPoint[0],
        endPoint[1] - vertexPoint[1],
        endPoint[2] - vertexPoint[2]
    };

    double len1 = vtkMath::Norm(v1.data());
    double len2 = vtkMath::Norm(v2.data());

    if (len1 < 1e-6 || len2 < 1e-6) return 0.0;  // 退化情况

    vtkMath::Normalize(v1.data());
    vtkMath::Normalize(v2.data());

    double dot = std::clamp(vtkMath::Dot(v1.data(), v2.data()), -1.0, 1.0);
    return vtkMath::DegreesFromRadians(std::acos(dot));
}

std::array<double, 3> SimpleAngleMeasureManager::ComputeAngleLabelPosition(
    const std::array<double, 3>& start,
    const std::array<double, 3>& vertex,
    const std::array<double, 3>& end,
    double offsetFactor)
{
    std::array<double, 3> toStart = { start[0] - vertex[0], start[1] - vertex[1], start[2] - vertex[2] };
    std::array<double, 3> toEnd = { end[0] - vertex[0], end[1] - vertex[1], end[2] - vertex[2] };

    double lenToStart = vtkMath::Norm(toStart.data());
    double lenToEnd = vtkMath::Norm(toEnd.data());

    if (lenToStart < 1e-6 || lenToEnd < 1e-6) {
        // 退化：直接偏移顶点
        auto pos = vertex;
        pos[0] += 10.0;
        return pos;
    }

    vtkMath::Normalize(toStart.data());
    vtkMath::Normalize(toEnd.data());

    // 角平分线方向 = 两单位向量之和
    std::array<double, 3> bisector = {
        toStart[0] + toEnd[0],
        toStart[1] + toEnd[1],
        toStart[2] + toEnd[2]
    };

    double bisLen = vtkMath::Norm(bisector.data());
    if (bisLen > 1e-6) {
        vtkMath::Normalize(bisector.data());
        double offset = std::min(lenToStart, lenToEnd) * offsetFactor;
        std::array<double, 3> pos;
        for (int i = 0; i < 3; ++i) {
            pos[i] = vertex[i] + bisector[i] * offset;
        }
        return pos;
    }
    else {
        // 180度平角：取垂直方向作为标签偏移
        std::array<double, 3> perp;
        if (std::abs(toStart[0]) > 0.1 || std::abs(toStart[1]) > 0.1) {
            perp = { -toStart[1], toStart[0], 0.0 };
        }
        else {
            perp = { 0.0, -toStart[2], toStart[1] };
        }
        vtkMath::Normalize(perp.data());
        std::array<double, 3> pos;
        for (int i = 0; i < 3; ++i) {
            pos[i] = vertex[i] + perp[i] * 10.0;
        }
        return pos;
    }
}

// ============================================================
//  弧线点生成（消除 CreateAngleArc 与预览代码重复）
// ============================================================

void SimpleAngleMeasureManager::GenerateArcPoints(
    const std::array<double, 3>& startPoint,
    const std::array<double, 3>& vertexPoint,
    const std::array<double, 3>& endPoint,
    vtkPoints* points,
    vtkPolyLine* polyLine)
{
    if (!points || !polyLine) return;

    points->Reset();
    polyLine->GetPointIds()->SetNumberOfIds(0);

    // 从顶点出发的两条边向量
    std::array<double, 3> v1 = {
        startPoint[0] - vertexPoint[0],
        startPoint[1] - vertexPoint[1],
        startPoint[2] - vertexPoint[2]
    };
    std::array<double, 3> v2 = {
        endPoint[0] - vertexPoint[0],
        endPoint[1] - vertexPoint[1],
        endPoint[2] - vertexPoint[2]
    };

    double r1 = vtkMath::Norm(v1.data());
    double r2 = vtkMath::Norm(v2.data());
    if (r1 < 1e-6 || r2 < 1e-6) return;

    vtkMath::Normalize(v1.data());
    vtkMath::Normalize(v2.data());

    double dot = std::clamp(vtkMath::Dot(v1.data(), v2.data()), -1.0, 1.0);
    double angle = std::acos(dot);
    if (angle < 1e-3) return;  // 两边几乎共线，无法绘制弧

    // 旋转轴 = v1 x v2
    double axis[3];
    vtkMath::Cross(v1.data(), v2.data(), axis);
    if (vtkMath::Norm(axis) < 1e-6) return;
    vtkMath::Normalize(axis);

    double radius = std::min(r1, r2) * 0.3;
    int    nSegments = std::max(5, static_cast<int>(angle * 10));

    // 用 Rodrigues 旋转公式生成弧线上各点
    double kx = axis[0], ky = axis[1], kz = axis[2];
    double vx = v1[0], vy = v1[1], vz = v1[2];
    double kdotv = kx * vx + ky * vy + kz * vz;  // 提前计算 k·v，循环内不变

    for (int i = 0; i <= nSegments; ++i) {
        double theta = static_cast<double>(i) / nSegments * angle;
        double cosT = std::cos(theta);
        double sinT = std::sin(theta);

        double x = vx * cosT + (ky * vz - kz * vy) * sinT + kx * kdotv * (1 - cosT);
        double y = vy * cosT + (kz * vx - kx * vz) * sinT + ky * kdotv * (1 - cosT);
        double z = vz * cosT + (kx * vy - ky * vx) * sinT + kz * kdotv * (1 - cosT);

        points->InsertNextPoint(
            vertexPoint[0] + radius * x,
            vertexPoint[1] + radius * y,
            vertexPoint[2] + radius * z);
    }

    // 更新 polyLine 连接关系
    vtkIdType numPoints = points->GetNumberOfPoints();
    polyLine->GetPointIds()->SetNumberOfIds(numPoints);
    for (vtkIdType i = 0; i < numPoints; ++i) {
        polyLine->GetPointIds()->SetId(i, i);
    }
}

// ============================================================
//  CreateAngleArc：直接复用 GenerateArcPoints，消除重复代码
// ============================================================

vtkSmartPointer<vtkActor> SimpleAngleMeasureManager::CreateAngleArc(
    const std::array<double, 3>& startPoint,
    const std::array<double, 3>& vertexPoint,
    const std::array<double, 3>& endPoint)
{
    vtkNew<vtkPoints>   points;
    vtkNew<vtkPolyLine> polyLine;
    GenerateArcPoints(startPoint, vertexPoint, endPoint, points, polyLine);

    if (points->GetNumberOfPoints() < 2) return nullptr;

    vtkNew<vtkCellArray> cells;
    cells->InsertNextCell(polyLine);

    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetLines(cells);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 0.5, 0.0);  // 橙色
    actor->GetProperty()->SetLineWidth(2);

    return actor;
}

// ============================================================
//  CreateAngleLabel
// ============================================================

vtkSmartPointer<vtkFollower> SimpleAngleMeasureManager::CreateAngleLabel(
    double angleDeg,
    const std::array<double, 3>& labelPosition,
    vtkCamera* camera)
{
    if (!camera) return nullptr;

    QString text = QString::number(angleDeg, 'f', 1) + "deg";

    vtkNew<vtkVectorText> textSource;
    textSource->SetText(text.toStdString().c_str());

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(textSource->GetOutputPort());

    vtkNew<vtkFollower> follower;
    follower->SetMapper(mapper);
    follower->SetPosition(labelPosition[0], labelPosition[1], labelPosition[2]);
    follower->SetScale(8.0, 8.0, 8.0);
    follower->SetCamera(camera);
    follower->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色

    return follower;
}

// ============================================================
//  基础图元创建
// ============================================================

vtkSmartPointer<vtkActor> SimpleAngleMeasureManager::createSphereActor(
    const std::array<double, 3>& point)
{
    auto sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter(point.data());
    sphereSource->SetRadius(0.5);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphereSource->GetOutputPort());

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 0.0, 0.0);  // 红色
    return actor;
}

vtkSmartPointer<vtkActor> SimpleAngleMeasureManager::createLineActor(
    const std::array<double, 3>& startPoint,
    const std::array<double, 3>& endPoint)
{
    auto lineSource = vtkSmartPointer<vtkLineSource>::New();
    lineSource->SetPoint1(startPoint.data());
    lineSource->SetPoint2(endPoint.data());

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(lineSource->GetOutputPort());

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 1.0, 0.0);  // 绿色
    return actor;
}

vtkSmartPointer<vtkActor> SimpleAngleMeasureManager::createCrosshairActor(
    const std::array<double, 3>& center, double length)
{
    // 三轴十字线
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
    actor->GetProperty()->SetColor(0.0, 1.0, 1.0);  // 青色
    actor->GetProperty()->SetLineWidth(2.0);
    return actor;
}