#pragma once

#include "common/ViewTypes.h"

class vtkRenderer;
class vtkImageViewer2;

/**
 * @brief Overlay 功能模块接口
 *
 * 每种标注功能（十字线、测距、测角、信息文字）均继承此接口，
 * 由 IOverlayManager 统一管理生命周期。
 *
 * 设计约定：
 *   - 所有 VTK Actor 在 Initialize() 中创建并加入渲染器
 *   - 所有 VTK Actor 在 Shutdown() 中从渲染器移除
 *   - OnSliceChanged() 在切片变化时由 Manager 统一调用
 */
class IOverlayFeature {
public:
    virtual ~IOverlayFeature() = default;

    /**
     * @brief 初始化，创建 VTK Actor 并加入渲染器
     * @param overlayRenderer Overlay 专用渲染器（第二层）
     */
    virtual void Initialize(vtkRenderer* overlayRenderer) = 0;

    /// @brief 设置可见性
    virtual void SetVisible(bool visible) = 0;

    /// @brief 设置标注颜色（RGB，各分量范围 0.0–1.0）
    virtual void SetColor(double r, double g, double b) = 0;

    /// @brief 释放 VTK 资源，将 Actor 从渲染器移除
    virtual void Shutdown() = 0;

    /**
     * @brief 切片变化时更新坐标
     *
     * 测量标注在切片切换后需要将坐标投影到新切片平面上，
     * 此方法由 SimpleOverlayManager::OnSliceChanged() 统一调用。
     *
     * @param viewer    VTK 图像查看器（用于读取 spacing / origin）
     * @param slice     新切片索引
     * @param viewType  变化的视图方向
     */
    virtual void OnSliceChanged(vtkImageViewer2* viewer,
        int slice,
        ViewType viewType) = 0;
};
