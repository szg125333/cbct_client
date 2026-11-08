#pragma once

#include "IOverlayFeature.h"
#include "common/EventData.h"
#include "common/RenderViewState.h"
#include "common/ROIDisplayInfo.h"
#include "utils/RTStructureData.h"  // 只依赖数据结构，不依赖具体 Feature

#include <typeinfo>
#include <array>
#include <memory> // 必须包含

class vtkRenderer;
class vtkImageViewer2;

/**
 * @brief Overlay 管理器接口
 *
 * 统一管理附加在切片视图上的所有 Overlay 功能（十字线、测距、测角、信息文字等）。
 * 各功能以 IOverlayFeature 的形式注册，管理器负责生命周期与统一驱动。
 *
 * 使用方式（Controller 侧）：
 *   auto mgr = OverlayFactory::CreateDefault();
 *   mgr->Initialize(overlayRenderer, viewer);
 *   renderer->SetOverlayManager(std::move(mgr));
 */
class IOverlayManager {
public:
    virtual ~IOverlayManager() = default;

    // ----------------------------------------------------------------
    //  生命周期
    // ----------------------------------------------------------------

    /**
     * @brief 初始化所有已注册的 Feature
     * @param overlayRenderer  Overlay 专用渲染器（第二层）
     * @param viewer           VTK 图像查看器（用于读取 spacing / origin 等信息）
     */
    virtual void Initialize(vtkRenderer* overlayRenderer, vtkImageViewer2* viewer) = 0;

    /// @brief 释放所有 VTK 资源
    virtual void Shutdown() = 0;

    // ----------------------------------------------------------------
    //  样式控制
    // ----------------------------------------------------------------

    virtual void SetVisible(bool visible) = 0;
    virtual void SetColor(double r, double g, double b) = 0;

    // ----------------------------------------------------------------
    //  图像边界
    // ----------------------------------------------------------------

    /**
     * @brief 设置图像的世界坐标包围盒（用于边界检查）
     * @param bounds [xMin,xMax, yMin,yMax, zMin,zMax]
     */
    virtual void SetImageWorldBounds(const std::array<double, 6>& bounds) = 0;

    /**
     * @brief 判断世界坐标点是否在图像范围内
     * @return 在范围内返回 true；未设置边界时保守返回 true
     */
    virtual bool IsWorldPointInImage(const std::array<double, 3>& worldPoint) const = 0;

    // ----------------------------------------------------------------
    //  驱动接口
    // ----------------------------------------------------------------

    /// @brief 处理交互事件（当前未使用，预留扩展）
    virtual bool Update(const EventData& event) = 0;

    /**
     * @brief 切片变化时通知所有 Feature 更新坐标
     * @param viewType  变化的视图方向
     * @param slice     新切片索引
     */
    virtual bool OnSliceChanged(ViewType viewType, int slice) = 0;

    /**
     * @brief 更新 Overlay 信息文字（窗宽窗位、切片号、坐标等）
     * @param state 当前视图状态快照
     */
    virtual void UpdateBasicInfoActor(const RenderViewState& state) = 0;

    /// @brief 设置 RT Structure 数据（通用接口）
    virtual void SetRTStructureData(std::shared_ptr<RTStructureData> data) = 0;

    // ----------------------------------------------------------------
    //  Feature 访问（模板，不需要虚函数）
    // ----------------------------------------------------------------

    /**
     * @brief 按类型获取已注册的 Feature
     * @tparam T Feature 的具体类型（必须继承自 IOverlayFeature）
     * @return 找到返回裸指针，否则返回 nullptr
     */
    template <typename T>
    T* GetFeature() {
        return dynamic_cast<T*>(GetFeatureImpl(typeid(T)));
    }

    /// 获取 ROI 列表（供 UI 显示）
    virtual std::vector<ROIDisplayInfo> GetROIList() const = 0;

    /// 设置某个 ROI 的可见性
    virtual void SetROIVisible(int roiNumber, bool visible) = 0;

protected:
    /// @brief 按 type_info 查找 Feature 的底层实现（供模板函数调用）
    virtual IOverlayFeature* GetFeatureImpl(const std::type_info& type) = 0;
};
