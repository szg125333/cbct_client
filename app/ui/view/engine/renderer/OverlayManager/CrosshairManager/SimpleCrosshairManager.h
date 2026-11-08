#pragma once

#include "renderer/OverlayManager/IOverlayFeature.h"
#include <array>
#include <vtkSmartPointer.h>

class vtkLineSource;
class vtkPolyDataMapper;
class vtkActor;

/**
 * @brief 十字线 Overlay Feature
 *
 * 在切片视图上绘制两条相互垂直的线，标记当前点击位置。
 * 三视图均使用此 Feature，每个视图独立维护一个实例。
 *
 * 线的端点由调用方传入图像世界坐标范围，确保十字线延伸到图像边缘。
 */
class SimpleCrosshairManager : public IOverlayFeature {
public:
    SimpleCrosshairManager();
    ~SimpleCrosshairManager() override;

    // IOverlayFeature 接口
    void Initialize(vtkRenderer* overlayRenderer) override;
    void SetVisible(bool visible)                 override;
    void SetColor(double r, double g, double b)   override;
    void Shutdown()                               override;
    void OnSliceChanged(vtkImageViewer2* viewer,
        int slice,
        ViewType viewType)        override;

    // ----------------------------------------------------------------
    //  十字线控制
    // ----------------------------------------------------------------

    /**
     * @brief 更新十字线位置
     * @param worldPoint 当前点击的世界坐标
     * @param viewType   当前视图方向（决定固定哪个轴）
     * @param worldMin   图像世界坐标包围盒最小值（3 个分量）
     * @param worldMax   图像世界坐标包围盒最大值（3 个分量）
     */
    void UpdateCrosshair(std::array<double, 3> worldPoint,
        ViewType viewType,
        const double worldMin[3],
        const double worldMax[3]);

    /// @brief 清除十字线（隐藏，不删除 Actor）
    void ClearAllMeasurement();

private:
    vtkRenderer* m_overlayRenderer = nullptr;

    vtkSmartPointer<vtkLineSource>      m_hLine;    ///< 水平线数据源
    vtkSmartPointer<vtkLineSource>      m_vLine;    ///< 垂直线数据源
    vtkSmartPointer<vtkPolyDataMapper>  m_hMapper;
    vtkSmartPointer<vtkPolyDataMapper>  m_vMapper;
    vtkSmartPointer<vtkActor>           m_hActor;   ///< 水平线 Actor
    vtkSmartPointer<vtkActor>           m_vActor;   ///< 垂直线 Actor

    bool m_initialized = false;
    bool m_visible = false;
    bool m_hasValidPoint = false;

    /// 最近一次有效世界坐标（切片变化后重绘用）
    std::array<double, 3> m_lastWorldPoint = { 0.0, 0.0, 0.0 };
    double m_lastImageMin[3] = { 0.0, 0.0, 0.0 };
    double m_lastImageMax[3] = { 0.0, 0.0, 0.0 };
};
