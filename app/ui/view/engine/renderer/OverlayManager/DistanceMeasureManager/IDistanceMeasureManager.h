#pragma once
#include <array>
#include "../IOverlayFeature.h"

class vtkRenderer;
class vtkImageViewer2;

/// @brief 距离测量工具管理器接口
class IDistanceMeasureManager :public IOverlayFeature {
public:
    virtual ~IDistanceMeasureManager() = default;

    //virtual void Initialize(vtkRenderer* overlayRenderer, vtkImageViewer2* viewer) = 0;
    virtual void StartMeasure(const std::array<double, 3>& startPoint) = 0;
    virtual void UpdateMeasure(const std::array<double, 3>& endPoint) = 0;
    virtual void EndMeasure() = 0;
    //virtual void SetVisible(bool visible) = 0;
    //virtual void Shutdown() = 0;

    virtual void DrawStartPoint(std::array<double, 3> worldPoint) = 0; // 新增方法
    virtual void DrawFinalMeasurementLine(std::array<double, 3> startPos, std::array<double, 3> endPos) = 0;
    virtual void PreviewMeasurementLine(std::array<double, 3> startPos, std::array<double, 3> currentPos) = 0;
    virtual void ClearAllMeasurement() = 0;
    virtual void ClearCurrentMeasurement() = 0;
};