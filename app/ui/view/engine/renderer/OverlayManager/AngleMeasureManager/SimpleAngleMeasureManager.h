#pragma once

#include "IAngleMeasureManager.h"
#include "common/Measurement/MeasurementTypes.h"

class vtkRenderer;
class vtkImageViewer2;
class vtkActor;
class vtkFollower;
class vtkLineSource;
class vtkPoints;
class vtkPolyLine;
class vtkCellArray;
class vtkPolyData;
class vtkVectorText;
class vtkCamera;

#include <vtkSmartPointer.h>
#include <array>
#include <unordered_map>
#include <vector>

/// @brief 角度测量线管理器实现
class SimpleAngleMeasureManager : public IAngleMeasureManager {
public:
    // === 生命周期管理 ===
    SimpleAngleMeasureManager();
    ~SimpleAngleMeasureManager() override;

    void Initialize(vtkRenderer* overlayRenderer) override;
    void Shutdown() override;
    void SetVisible(bool visible) override;
    void OnSliceChanged(vtkImageViewer2* viewer, int slice, ViewType viewType) override;

    // === 测量流程控制 ===
    void StartMeasure(const std::array<double, 3>& point1) override;
    void UpdateMeasure(const std::array<double, 3>& point2) override;
    void EndMeasure(const std::array<double, 3>& point3) override;

    // === 样式 ===
    void SetColor(double r, double g, double b) override;

    // === 清除 ===
    void ClearAllMeasurement() override;
    void ClearCurrentMeasurement() override;

    /// 导出用：当前全部角度测量（含未完成）
    std::vector<MeasurementRecord> GetMeasurementRecords() const;

    // === 点编辑支持（供外部交互使用）===
    EditableAnglePoint GetEditableAnglePoint(int screenX, int screenY) const;
    void UpdateAngleMeasurementPoint(int measurementId, AnglePointRole role, const std::array<double, 3>& newWorldPos);

    // === 静态计算工具（供外部调用）===
    static double ComputeAngle(
        const std::array<double, 3>& startPoint,
        const std::array<double, 3>& vertexPoint,
        const std::array<double, 3>& endPoint);

    static std::array<double, 3> ComputeAngleLabelPosition(
        const std::array<double, 3>& start,
        const std::array<double, 3>& vertex,
        const std::array<double, 3>& end,
        double offsetFactor = 0.3);

    // === 绘制入口（由 StartMeasure/UpdateMeasure/EndMeasure 调用）===
    void DrawStartPoint(const std::array<double, 3>& worldPoint);
    void DrawMiddlePointAndStartToMiddleLine(const std::array<double, 3>& worldPoint);
    void DrawEndPointAndMiddleToEndLine(const std::array<double, 3>& worldPoint);

    // === 预览更新 ===
    void PreviewStartToMiddleMeasurementLine(const std::array<double, 3>& startPos, const std::array<double, 3>& currentPos);
    void PreviewMiddleToEndMeasurementLine(const std::array<double, 3>& startPos, const std::array<double, 3>& currentPos);
    void ClearPreview();

private:

    // === 预览弧线/标签更新（懒初始化，零重建）===
    void UpdatePreviewAngleArc(
        const std::array<double, 3>& start,
        const std::array<double, 3>& vertex,
        const std::array<double, 3>& end);

    void UpdatePreviewAngleLabel(
        double angleDeg,
        const std::array<double, 3>& start,
        const std::array<double, 3>& vertex,
        const std::array<double, 3>& end,
        vtkCamera* camera);

    // === 最终测量绘制（新建或重建全部 actor）===
    void DrawFinalAngleMeasurement(int measurementId);

    // === 弧线点生成（被 CreateAngleArc 和预览共用）===
    void GenerateArcPoints(
        const std::array<double, 3>& startPoint,
        const std::array<double, 3>& vertexPoint,
        const std::array<double, 3>& endPoint,
        vtkPoints* points,
        vtkPolyLine* polyLine);

    // === 创建弧线 Actor（内部复用 GenerateArcPoints）===
    vtkSmartPointer<vtkActor> CreateAngleArc(
        const std::array<double, 3>& startPoint,
        const std::array<double, 3>& vertexPoint,
        const std::array<double, 3>& endPoint);

    // === 创建角度标签 Follower ===
    vtkSmartPointer<vtkFollower> CreateAngleLabel(
        double angleDeg,
        const std::array<double, 3>& labelPosition,
        vtkCamera* camera);

    // === 基础图元创建 ===
    vtkSmartPointer<vtkActor> createSphereActor(const std::array<double, 3>& point);
    vtkSmartPointer<vtkActor> createLineActor(const std::array<double, 3>& p1, const std::array<double, 3>& p2);
    vtkSmartPointer<vtkActor> createCrosshairActor(const std::array<double, 3>& center, double length = 10.0);

    // === 移除单个测量的所有 actor 辅助函数 ===
    void removeMeasurementActors(int measurementId);

    // === 数据结构 ===
    struct Measurement {
        int id = -1;

        // 三个控制点世界坐标
        std::array<double, 3> startPointWorld = {};
        std::array<double, 3> middlePointWorld = {};
        std::array<double, 3> endPointWorld = {};

        // 可视化 Actor
        vtkSmartPointer<vtkActor>   startPointActor;
        vtkSmartPointer<vtkActor>   startCrosshairActor;
        vtkSmartPointer<vtkActor>   middlePointActor;
        vtkSmartPointer<vtkActor>   middleCrosshairActor;
        vtkSmartPointer<vtkActor>   endPointActor;
        vtkSmartPointer<vtkActor>   endCrosshairActor;
        vtkSmartPointer<vtkActor>   startToMiddleLineActor;
        vtkSmartPointer<vtkActor>   middleToEndLineActor;
        vtkSmartPointer<vtkActor>   angleArcActor;
        vtkSmartPointer<vtkFollower> angleLabel;

        bool isComplete = false;
    };

    // === 成员变量 ===
    std::unordered_map<int, Measurement> m_measurements;
    int m_currentId = 0;  // 当前正在绘制的测量 ID
    int generateNextId() { return ++m_currentId; }

    // 预览专用对象（不在 m_measurements 中，懒初始化）
    vtkSmartPointer<vtkLineSource> m_previewStartToMiddleLineSource;
    vtkSmartPointer<vtkActor>      m_previewStartToMiddleLineActor;
    vtkSmartPointer<vtkLineSource> m_previewMiddleToEndLineSource;
    vtkSmartPointer<vtkActor>      m_previewMiddleToEndLineActor;

    vtkSmartPointer<vtkActor>      m_previewAngleArcActor;
    vtkSmartPointer<vtkPoints>     m_previewArcPoints;
    vtkSmartPointer<vtkPolyLine>   m_previewArcPolyLine;
    vtkSmartPointer<vtkCellArray>  m_previewArcCells;
    vtkSmartPointer<vtkPolyData>   m_previewArcPolyData;
    vtkSmartPointer<vtkFollower>   m_previewAngleLabel;
    vtkSmartPointer<vtkVectorText> m_previewLabelText;

    vtkSmartPointer<vtkRenderer> m_overlayRenderer;
    vtkImageViewer2* m_viewer = nullptr;
    bool m_initialized = false;
    bool m_visible = true;
};