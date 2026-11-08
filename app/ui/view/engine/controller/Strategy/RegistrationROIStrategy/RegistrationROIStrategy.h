#pragma once

#include "../AbstractMeasureStrategy.h"
#include "common/ViewTypes.h"
#include "renderer/OverlayManager/ROIManager/SimpleROIManager.h"

/**
 * @brief ROI 矩形框绘制 + 拖动交互策略
 *
 * ──────────────────────────────────────────────
 *  绘制新 ROI（未命中已有 ROI 时）
 * ──────────────────────────────────────────────
 *   左键按下         → BeginROI()
 *   左键拖拽         → UpdatePreview() 实时刷新虚线框
 *   左键释放         → CommitROI() 固定矩形 + 统计标签显示在框下方
 *   右键 / Esc 键    → CancelCurrentROI()
 *
 * ──────────────────────────────────────────────
 *  拖动已有 ROI（命中已有 ROI 时）
 * ──────────────────────────────────────────────
 *   命中边框或内部   → 整体平移（MoveROI）
 *   命中四个角点     → 角点缩放（ResizeROI）
 *   操作期间统计实时更新
 *
 * ──────────────────────────────────────────────
 *  删除
 * ──────────────────────────────────────────────
 *   Delete 键        → DeleteLastROI()
 *   Ctrl + Delete    → ClearAllROI()
 */
class RegistrationROIStrategy : public AbstractMeasureStrategy {
public:
    explicit RegistrationROIStrategy(IViewController* controller);

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    // ----------------------------------------------------------------
    //  状态机
    // ----------------------------------------------------------------

    /**
     * @brief 策略当前所处的工作阶段
     */
    enum class Phase {
        Idle,       ///< 空闲，等待下一次操作
        Drawing,    ///< 正在绘制新 ROI（左键按住拖拽中）
        Moving,     ///< 正在整体平移已有 ROI
        Resizing,   ///< 正在缩放已有 ROI（拖拽角点）
    };

    Phase m_phase = Phase::Idle;

    // ---- 绘制阶段状态 ----
    ViewType m_drawViewType = ViewType::None;
    int      m_drawSlice = 0;

    // ---- 拖动阶段状态 ----
    int        m_dragRoiId = -1;              ///< 正在拖动的 ROI ID
    RoiHitType m_dragHitType = RoiHitType::None;///< 命中类型（整体/角点）

    /// 上一帧鼠标世界坐标（平移时计算增量用）
    std::array<double, 3> m_lastDragWorldPos = { 0, 0, 0 };
};