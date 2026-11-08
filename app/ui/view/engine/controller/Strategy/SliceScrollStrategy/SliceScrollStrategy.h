#pragma once

#include "controller/Strategy/IInteractionStrategy.h"

/// @brief 切片滚动策略
///
/// 选中该模式后，在任意视图上按住左键上下拖动即可快速切换切片。
/// 向上拖动 → 切片号增大，向下拖动 → 切片号减小。
/// 只影响当前拖动的视图，不联动其他视图。
class SliceScrollStrategy : public IInteractionStrategy {
public:
    explicit SliceScrollStrategy(IViewController* controller);

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    bool m_dragging = false;
    int m_lastY = 0;           // 上一帧鼠标 Y 坐标
    int m_activeView = -1;     // 正在拖动的视图索引
    int m_accumDelta = 0;      // 累积的像素偏移（用于灵敏度控制）
    int m_sensitivity = 6;     // 每移动多少像素切换一个切片
};