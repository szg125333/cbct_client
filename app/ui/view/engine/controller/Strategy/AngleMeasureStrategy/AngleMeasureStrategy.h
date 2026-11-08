#pragma once

#include "../AbstractMeasureStrategy.h"

/**
 * @brief 角度测量交互策略
 *
 * 操作流程（三点定角）：
 *   1. 左键第一次点击 → 放置起始点
 *   2. 移动鼠标       → 预览起始点到顶点的连线
 *   3. 左键第二次点击 → 放置顶点（角的顶点）
 *   4. 移动鼠标       → 预览顶点到终止点的连线
 *   5. 左键第三次点击 → 放置终止点，完成测量并显示角度值
 *   6. 右键           → 取消当前未完成的测量
 *   7. Ctrl + 左键拖拽已有端点 → 编辑端点位置
 */
class AngleMeasureStrategy : public AbstractMeasureStrategy {
public:
    explicit AngleMeasureStrategy(IViewController* controller);

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    /**
     * @brief 测量进度枚举
     *
     * 记录当前处于三点定角的哪个阶段。
     */
    enum class MeasureStep {
        Idle,        ///< 空闲，等待第一次点击
        StartPlaced, ///< 起始点已放置，等待顶点
        VertexPlaced ///< 顶点已放置，等待终止点
    };

    MeasureStep m_step = MeasureStep::Idle;

    /// 顶点（角的顶点）世界坐标
    std::array<double, 3> m_vertexWorldPos = { 0.0, 0.0, 0.0 };

    /// 正在编辑的端点角色（start / middle / end）
    int m_editingPointRole = -1;
};
