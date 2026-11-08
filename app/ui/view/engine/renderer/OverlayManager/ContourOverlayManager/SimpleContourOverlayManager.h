#pragma once

#include "renderer/OverlayManager/IOverlayFeature.h"
#include "utils/RTStructureData.h"
#include "common/ViewTypes.h"
#include "common/ROIDisplayInfo.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkRenderer.h>

#include <vector>
#include <map>
#include <memory>

class vtkImageViewer2;

/// @brief RT Structure 轮廓叠加显示管理器
///
/// 三个视图统一用几何切割方式：
/// - Axial：匹配 Z 坐标，直接用原始轮廓点画闭合折线
/// - Sagittal/Coronal：用切割平面与每条轮廓多边形求交，
///   每条轮廓产生若干交点，同一层的交点按排序后两两配对画短线段，
///   层与层之间不连接，彻底避免飞线
class SimpleContourOverlayManager : public IOverlayFeature {
public:
    SimpleContourOverlayManager();
    ~SimpleContourOverlayManager() override;

    void Initialize(vtkRenderer* overlayRenderer) override;
    void SetVisible(bool visible) override;
    void SetColor(double r, double g, double b) override;
    void Shutdown() override;
    void OnSliceChanged(vtkImageViewer2* viewer, int slice, ViewType viewType) override;

    void SetRTStructureData(std::shared_ptr<RTStructureData> data);
    void SetROIVisible(int roiNumber, bool visible);
    void ClearAllContours();
    std::vector<ROIDisplayInfo> GetROIList() const;

private:
    /// Axial：原始点画闭合折线
    void updateAxialContours(double sliceZ, double tolerance);

    /// Sagittal/Coronal：切割每条轮廓多边形，每层独立画线段
    void updateNonAxialContours(int cutAxis, double slicePos, double tolerance);

    /// 计算闭合多边形与平面 axis=slicePos 的交点
    void computeIntersections(const RTContour& contour, int axis, double slicePos,
        std::vector<std::array<double, 3>>& outPoints);

    /// 创建闭合/开放折线 Actor
    vtkSmartPointer<vtkActor> createLineActor(
        const std::vector<std::array<double, 3>>& points,
        const std::array<double, 3>& color, bool closed);

    /// 创建一条线段 Actor（两个点）
    vtkSmartPointer<vtkActor> createSegmentActor(
        const std::array<double, 3>& p1, const std::array<double, 3>& p2,
        const std::array<double, 3>& color);

    void removeAllActors();

    vtkSmartPointer<vtkRenderer> m_overlayRenderer;
    bool m_initialized = false;
    bool m_visible = true;

    std::shared_ptr<RTStructureData> m_rtData;
    std::map<int, bool> m_roiVisibility;

    std::vector<vtkSmartPointer<vtkActor>> m_currentActors;
};