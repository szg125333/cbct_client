#pragma once

#include "controller/Strategy/IInteractionStrategy.h"
#include "renderer/OverlayManager/RulerLineManager/SimpleRulerLineManager.h"
#include <array>

/**
 * @brief 双线独立刻度尺交互策略
 *
 * 操作说明：
 *
 *   普通左键拖动：
 *     命中水平线 → 上下移动水平线
 *     命中垂直线 → 左右移动垂直线
 *     未命中     → 忽略
 *
 *   Ctrl + 左键拖动（按下时即进入缩放模式，不管有没有命中刻度线）：
 *     鼠标上移（OpenGL Y 增大）→ 放大
 *     鼠标下移（OpenGL Y 减小）→ 缩小
 *     缩放以按下时的鼠标世界坐标为基准（焦点不偏移）
 *     灵敏度由 kZoomSensitivity 控制（默认 0.01，与 NormalStrategy 一致）
 *
 *   普通滚轮：切片（仍由 NormalStrategy 处理，ThreeViewController 固定路由）
 */
class RulerLineStrategy : public IInteractionStrategy {
public:
    explicit RulerLineStrategy(IViewController* controller);

    void OnActivated() override;
    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    // ---- 当前拖动的操作类型 ----
    enum class DragMode {
        None,       ///< 未按下
        MoveRuler,  ///< 移动刻度线
        Zoom,       ///< Ctrl + 拖动缩放
    };

    DragMode m_dragMode = DragMode::None;
    int m_activeViewIndex = -1;
    int m_lastScreenX = 0;
    int m_lastScreenY = 0;

    /// 缩放时的基准焦点（LeftPress 时记录，保持缩放中心不偏移）
    std::array<double, 3> m_zoomFocalPoint = { 0, 0, 0 };

    /// 移动刻度线时命中的是哪条线
    SimpleRulerLineManager::HitLine m_dragTarget =
        SimpleRulerLineManager::HitLine::None;

    /// 缩放灵敏度，与 NormalStrategy 保持一致
    static constexpr double kZoomSensitivity = 0.01;
};
