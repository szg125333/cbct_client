#pragma once
#include <array>
#include "../IOverlayFeature.h"

class vtkRenderer;
class vtkImageViewer2;

/// @brief 角度测量工具管理器接口
class IAngleMeasureManager :public IOverlayFeature {
public:
    virtual ~IAngleMeasureManager() = default;

    virtual void StartMeasure(const std::array<double, 3>& point1) = 0;
    virtual void UpdateMeasure(const std::array<double, 3>& point2) = 0;
    virtual void EndMeasure(const std::array<double, 3>& point3) = 0;

    virtual void ClearAllMeasurement() = 0;
    virtual void ClearCurrentMeasurement() = 0;
};