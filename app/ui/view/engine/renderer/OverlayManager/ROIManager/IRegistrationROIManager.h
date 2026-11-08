#pragma once
#include <array>

class vtkRenderer;
class vtkImageViewer2;

/// @brief 配准 ROI（盒子）管理器接口
class IRegistrationROIManager {
public:
    virtual ~IRegistrationROIManager() = default;

    virtual void Initialize(vtkRenderer* overlayRenderer, vtkImageViewer2* viewer) = 0;
    virtual void StartSelection() = 0;
    virtual void UpdateROI(const std::array<double, 3>& minPoint,
        const std::array<double, 3>& maxPoint) = 0;
    virtual void EndSelection() = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void Shutdown() = 0;
};