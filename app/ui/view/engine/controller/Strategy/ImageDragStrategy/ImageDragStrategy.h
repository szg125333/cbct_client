#pragma once

#include "../IInteractionStrategy.h"
#include <array>
#include <functional>

/**
 * @brief 图像平移拖动策略（无抖动版）
 *
 * 修正说明：
 *   原版用 PickWorldPosition 计算帧间世界坐标增量，因为 Picker
 *   依赖上一帧的深度缓冲区，在相机移动后渲染未及时刷新时会读到
 *   旧深度数据，导致坐标偏差累积 → 果冻抖动。
 *
 *   修正：改用屏幕像素增量 + ParallelScale 换算为世界坐标增量，
 *   完全不依赖深度缓冲，每帧结果稳定。
 *
 *   换算公式（正交投影）：
 *     worldPerPixel = (2 × ParallelScale) / viewportHeightPixels
 *     worldDelta    = pixelDelta × worldPerPixel
 *
 * 操作：
 *   左键按下 + 拖动 → 平移图像
 *   右键单击        → 累计位移清零
 */
class ImageDragStrategy : public IInteractionStrategy {
public:
    using DragCallback = std::function<void(int, double, double, double, double)>;
    using ResetCallback = std::function<void()>;

    explicit ImageDragStrategy(IViewController* controller);

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

    void SetDragCallback(DragCallback cb) { m_dragCb = std::move(cb); }
    void SetResetCallback(ResetCallback cb) { m_resetCb = std::move(cb); }

private:
    void ResetDisplacement();

    bool m_isDragging = false;
    int  m_activeViewIndex = -1;

    /// 上一帧的屏幕像素坐标（注意：不再是世界坐标）
    int m_lastScreenX = 0;
    int m_lastScreenY = 0;

    /// 累计物理位移（mm）
    double m_totalDx = 0.0;
    double m_totalDy = 0.0;
    double m_totalDz = 0.0;

    DragCallback  m_dragCb;
    ResetCallback m_resetCb;
};
