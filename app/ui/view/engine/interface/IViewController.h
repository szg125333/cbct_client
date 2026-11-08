#pragma once

#include "common/ViewTypes.h"
#include <array>

class IViewRenderer;
class vtkImageData;

/**
 * @brief 视图控制器接口
 *
 * 管理三视图（轴状/矢状/冠状）的全局状态，包括：
 * - 切片同步
 * - 窗宽窗位
 * - 缩放操作
 * - 交互策略调度
 *
 * Strategy 通过此接口与 Controller 通信，无需依赖具体实现。
 */
class IViewController {
public:
    virtual ~IViewController() = default;

    // ----------------------------------------------------------------
    //  切片控制
    // ----------------------------------------------------------------

    /**
     * @brief 在指定视图中增减切片
     * @param viewIndex 视图索引（0=轴状 1=矢状 2=冠状）
     * @param delta     偏移量（+1 下一层 / -1 上一层）
     */
    virtual void ChangeSlice(int viewIndex, int delta) = 0;

    /**
     * @brief 根据世界坐标同步三视图切片位置
     * @param worldPoint 点击的世界坐标
     */
    virtual void UpdateSliceByWorldPoint(std::array<double, 3> worldPoint) = 0;

    // ----------------------------------------------------------------
    //  窗宽窗位
    // ----------------------------------------------------------------

    /// @brief 获取当前窗宽
    virtual double GetWindowWidth() const = 0;

    /// @brief 获取当前窗位
    virtual double GetWindowLevel() const = 0;

    /// @brief 同时设置窗宽和窗位（广播到三视图）
    virtual void SetWindowLevel(double window, double level) = 0;

    // ----------------------------------------------------------------
    //  缩放
    // ----------------------------------------------------------------

    /**
     * @brief 以指定世界坐标为焦点缩放视图
     * @param viewIndex        目标视图索引
     * @param factor           缩放系数（>1 放大，<1 缩小）
     * @param focalWorldPoint  缩放中心的世界坐标（鼠标按下点）
     */
    virtual void Zoom(int viewIndex, double factor,
        std::array<double, 3> focalWorldPoint) = 0;

    // ----------------------------------------------------------------
    //  渲染器访问
    // ----------------------------------------------------------------

    /**
     * @brief 获取指定视图的渲染器
     * @param viewIndex 视图索引（0–2）
     * @return IViewRenderer 指针，索引越界时返回 nullptr
     */
    virtual IViewRenderer* GetRenderer(int viewIndex) = 0;

    // ----------------------------------------------------------------
    //  图像数据
    // ----------------------------------------------------------------

    /// @brief 获取当前加载的 VTK 图像数据（只读）
    virtual const vtkImageData* GetImage() const = 0;
};
