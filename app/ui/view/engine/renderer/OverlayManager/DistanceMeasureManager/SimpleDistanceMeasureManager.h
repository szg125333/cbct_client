#pragma once

#include "IDistanceMeasureManager.h"
#include "common/Measurement/MeasurementTypes.h"
#include <vtkSmartPointer.h>
#include <array>
#include <vector>
#include <vtkActor.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include "../IOverlayFeature.h"
#include <vtkVectorText.h>
#include <vtkFollower.h>
#include <vtkProperty.h>

class vtkRenderer;
class vtkImageViewer2;

// 👇 放在类外（或命名空间内）
struct EditablePoint {
    int measurementId = -1;
    bool isStart = true;
};

/// @brief 距离测量工具管理器实现
class SimpleDistanceMeasureManager : public IDistanceMeasureManager
{
public:
    SimpleDistanceMeasureManager();
    ~SimpleDistanceMeasureManager() override;

    void Initialize(vtkRenderer* overlayRenderer) override;
    void StartMeasure(const std::array<double, 3>& startPoint) override;
    void UpdateMeasure(const std::array<double, 3>& endPoint) override;
    void EndMeasure() override;
    void SetVisible(bool visible) override;
    void Shutdown() override;
    void SetColor(double r, double g, double b) override;
    void OnSliceChanged(vtkImageViewer2* viewer,int slice,ViewType viewType) override;

    void DrawStartPoint(std::array<double, 3> worldPoint) override;
    void DrawFinalMeasurementLine(std::array<double, 3> startPos, std::array<double, 3> endPos) override;
    void DrawFinalMeasurementLine(int measurementId,std::array<double, 3> startPos, std::array<double, 3> endPos);
    void PreviewMeasurementLine(std::array<double, 3> startPos, std::array<double, 3> currentPos) override;
    void ClearAllMeasurement() override;
    void ClearCurrentMeasurement() override;
    void ClearPreview();

    EditablePoint GetEditablePoint(int screenX, int screenY) const;
    void UpdateMeasurementPoint(int measurementId, bool isStart, const std::array<double, 3>& newWorldPos);

    /// 导出用：当前全部距离测量（含未完成）
    std::vector<MeasurementRecord> GetMeasurementRecords() const;

    //bool IsWorldPointInImage(const std::array<double, 3>& worldPoint) const;
private:

    vtkSmartPointer<vtkActor> createSphereActor(const std::array<double, 3>& point);
    vtkSmartPointer<vtkActor> createLineActor(const std::array<double, 3>& startPoint, const std::array<double, 3>& endPoint);
    vtkSmartPointer<vtkActor> createCrosshairActor(const std::array<double, 3>& center, double length = 10.0);
    vtkSmartPointer<vtkActor> createTickActor(const std::array<double, 3>& p1, const std::array<double, 3>& p2, double tickLength = 3.0);
    vtkSmartPointer<vtkFollower> createDistanceLabel(const std::array<double, 3>& p1, const std::array<double, 3>& p2, vtkCamera* camera, double scale = 8.0, double offset = 2.0);

    vtkSmartPointer<vtkRenderer> m_overlayRenderer;
    bool m_initialized = false;
    bool m_visible = true;

    //using MeasurementID = int;
    struct Measurement {
        int id;
        std::array<double, 3> startPointWorld;
        std::array<double, 3> endPointWorld;

        // 正式测量的 actors（不可更新，EndMeasure 后固定）
        vtkSmartPointer<vtkActor> startPointActor;
        vtkSmartPointer<vtkActor> endPointActor;
        vtkSmartPointer<vtkActor> startCrosshairActor;
        vtkSmartPointer<vtkActor> endCrosshairActor;
        vtkSmartPointer<vtkActor> lineActor;
        vtkSmartPointer<vtkActor> tickActor;
        vtkSmartPointer<vtkFollower> distanceLabel;

        bool isComplete = false;
    };

    std::unordered_map<int, Measurement> m_measurements;
    int m_nextId = 0;
    int generateNextId() { return ++m_nextId; }

    // === 新增：预览专用可复用组件（不存入 m_measurements）===
    vtkSmartPointer<vtkLineSource> m_previewLineSource;
    vtkSmartPointer<vtkActor>      m_previewLineActor;

    vtkSmartPointer<vtkLineSource> m_previewTickSource;
    vtkSmartPointer<vtkActor>      m_previewTickActor;

    vtkSmartPointer<vtkVectorText> m_previewTextSource;
    vtkSmartPointer<vtkFollower>   m_previewLabelActor;

    // 辅助更新函数
    void updatePreviewTick(const std::array<double, 3>& p1, const std::array<double, 3>& p2, double tickLength = 3.0);
    void updatePreviewLabel(const std::array<double, 3>& p1, const std::array<double, 3>& p2, vtkCamera* camera);
};