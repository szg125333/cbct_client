#pragma once

#include "../IInteractionStrategy.h"
#include <array>

/**
 * @brief 普通浏览策略
 *
 * 默认交互模式，处理：
 *   - 滚轮：切换切片
 *   - 左键拖拽：调整窗宽窗位（右→增大窗宽，上→增大窗位）
 *   - Ctrl + 左键拖拽：缩放视图
 *   - 左键单击：定位十字线并同步三视图切片
 */
class NormalStrategy : public IInteractionStrategy {
public:
    explicit NormalStrategy(IViewController* controller)
        : IInteractionStrategy(controller)
    {
    }

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    /// @brief 将当前窗宽窗位广播到三视图
    void ApplyWindowLevel(int viewIndex);

    /// @brief 根据点击坐标更新十字线并同步三视图切片位置
    void LocateCrosshair(int viewIndex, int screenX, int screenY);

    // ----------------------------------------------------------------
    //  状态
    // ----------------------------------------------------------------

    bool m_isDragging = false;

    /// 上一帧鼠标位置，用于计算拖拽增量
    int m_lastMousePos[2] = { 0, 0 };

    /// 拖拽开始时的世界坐标焦点（用于缩放时保持焦点不动）
    std::array<double, 3> m_dragStartWorldPos = { 0.0, 0.0, 0.0 };

    /// 拖拽开始时的窗宽 / 窗位快照
    double m_dragStartWindow = 0.0;
    double m_dragStartLevel = 0.0;

    /// 窗宽窗位调整灵敏度（每像素对应的调整量）
    static constexpr double kWindowSensitivity = 3.0;
    static constexpr double kLevelSensitivity = 3.0;
};
