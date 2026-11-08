#pragma once

#include "../AbstractMeasureStrategy.h"

/**
 * @brief 距离测量交互策略
 *
 * 操作流程：
 *   1. 左键第一次点击 → 放置起始端点
 *   2. 移动鼠标      → 实时预览测量线
 *   3. 左键第二次点击 → 完成测量，固定标注
 *   4. 右键           → 取消当前未完成的测量
 *   5. Ctrl + 左键拖拽已有端点 → 编辑端点位置
 */
class DistanceMeasureStrategy : public AbstractMeasureStrategy {
public:
    explicit DistanceMeasureStrategy(IViewController* controller);

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

private:
    /// 是否已记录第一个端点（等待第二次点击）
    bool m_hasFirstPoint = false;

    /// 正在编辑的端点是否为起始点（false = 终止点）
    bool m_editingIsStartPoint = false;
};
