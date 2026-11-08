#pragma once

#include "controller/Strategy/AbstractMeasureStrategy.h"
#include "common/ViewTypes.h"
#include "renderer/OverlayManager/FreehandROIManager/SimpleFreehandROIManager.h"

/**
 * @brief 自由手绘 ROI 交互策略（含拖动支持）
 *
 * 状态机：
 *
 *   Idle ──左键按下──→ HitTest
 *                        ├── 命中已有 ROI → Moving（整体平移）
 *                        └── 未命中       → Drawing（新绘制）
 *
 *   Drawing ──左键拖动──→ AddPoint
 *           ──左键释放──→ CommitFreehand → Idle
 *           ──右键/Esc ──→ CancelFreehand → Idle
 *
 *   Moving  ──左键拖动──→ MoveROI（实时平移）
 *           ──左键释放──→ Idle
 *
 * 键盘：
 *   Delete      → 删除最后一个 ROI
 *   Ctrl+Delete → 清除全部 ROI
 *   Esc         → 取消当前绘制（Drawing 状态下有效）
 */
class FreehandROIStrategy : public AbstractMeasureStrategy {
public:
    explicit FreehandROIStrategy(IViewController* controller);

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    enum class Phase {
        Idle,     ///< 空闲
        Drawing,  ///< 正在绘制新轮廓
        Moving,   ///< 正在拖动已有 ROI
    };

    Phase    m_phase = Phase::Idle;
    ViewType m_drawViewType = ViewType::None;
    int      m_drawSlice = 0;

    // ---- 拖动状态 ----
    int                   m_dragRoiId = -1;
    std::array<double, 3> m_lastDragPos = { 0, 0, 0 };  ///< 上一帧世界坐标
};
