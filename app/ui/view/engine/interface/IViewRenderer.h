#pragma once

#include "common/ViewTypes.h"
#include "common/EventData.h"
#include <array>
#include <functional>
#include <memory>

class vtkImageData;
class vtkImageViewer2;
class vtkRenderer;
class IOverlayManager;

template <typename T> class vtkSmartPointer;

/**
 * @brief 视图渲染器接口
 *
 * 定义单个切片视图（轴状/矢状/冠状）的渲染能力。
 * 上层 Controller 通过此接口驱动渲染器，无需关心具体 VTK 实现。
 */
class IViewRenderer {
public:
    virtual ~IViewRenderer() = default;

    // ----------------------------------------------------------------
    //  图像数据
    // ----------------------------------------------------------------

    /// @brief 设置待显示的 VTK 图像数据
    virtual void SetInputData(vtkImageData* image) = 0;

    // ----------------------------------------------------------------
    //  切片控制
    // ----------------------------------------------------------------

    /// @brief 设置切片方向（轴状 / 矢状 / 冠状）
    virtual void SetOrientation(ViewType viewType) = 0;

    /// @brief 设置当前切片索引
    virtual void SetSlice(int slice) = 0;

    /// @brief 设置最大切片数（用于 Overlay 信息显示）
    virtual void SetMaxSlice(int maxSlice) = 0;

    /// @brief 获取当前切片索引
    virtual int  GetSlice() const = 0;

    // ----------------------------------------------------------------
    //  窗宽窗位
    // ----------------------------------------------------------------

    /// @brief 设置窗宽
    virtual void SetColorWindow(double window) = 0;

    /// @brief 设置窗位
    virtual void SetColorLevel(double level) = 0;

    // ----------------------------------------------------------------
    //  坐标拾取
    // ----------------------------------------------------------------

    /**
     * @brief 将屏幕坐标转换为世界坐标
     * @param screenX  屏幕 X（像素）
     * @param screenY  屏幕 Y（像素）
     * @return 世界坐标，拾取失败时返回 NaN
     */
    virtual std::array<double, 3> PickWorldPosition(int screenX, int screenY) = 0;

    /// @brief 设置最近一次点击的世界坐标（供 Overlay 信息层使用）
    virtual void SetCurrentClickWorldPos(std::array<double, 3> worldPos) = 0;

    // ----------------------------------------------------------------
    //  渲染控制
    // ----------------------------------------------------------------

    /**
     * @brief 请求渲染（批量合并，16 ms 内只渲染一次）
     *
     * 多处同时调用时仅触发一次真实渲染，避免单帧重复绘制。
     */
    virtual void RequestRender() = 0;

    // ----------------------------------------------------------------
    //  Overlay 信息刷新
    // ----------------------------------------------------------------

    /// @brief 收集当前状态并发出 viewStateChanged 信号，驱动 Overlay 信息层更新
    virtual void UpdateBasicInfoActor() = 0;

    // ----------------------------------------------------------------
    //  底层对象访问（供 Controller 和 Strategy 使用）
    // ----------------------------------------------------------------

    /// @brief 获取 VTK 图像查看器
    virtual vtkSmartPointer<vtkImageViewer2> GetViewer() = 0;

    /// @brief 获取 Overlay 专用渲染器（第二层，不参与图像渲染）
    virtual vtkSmartPointer<vtkRenderer>     GetOverlayRenderer() = 0;

    /// @brief 获取 Overlay 管理器
    virtual IOverlayManager* GetOverlayManager() = 0;

    /// @brief 设置 Overlay 管理器（仅应调用一次）
    virtual void SetOverlayManager(std::unique_ptr<IOverlayManager> manager) = 0;

    // ----------------------------------------------------------------
    //  事件注册
    // ----------------------------------------------------------------

    /**
     * @brief 注册 VTK 事件回调
     * @param type 事件类型
     * @param cb   回调函数；传 nullptr 可注销
     */
    virtual void OnEvent(EventType type, std::function<void(const EventData&)> cb) = 0;

    virtual ViewType GetCurrentViewType() = 0;
};
