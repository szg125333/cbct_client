#pragma once

#include "IInteractionStrategy.h"
#include <array>

/**
 * @brief 测量交互策略公共基类
 *
 * 封装距离测量和角度测量策略共享的逻辑：
 *   - 视图锁定（同一次测量只在一个视图内完成）
 *   - 图像边界检查（点击到图像外直接忽略）
 *   - 第一点记录
 *   - 编辑（拖拽已有测量端点）状态管理
 */
class AbstractMeasureStrategy : public IInteractionStrategy {
public:
    explicit AbstractMeasureStrategy(IViewController* controller)
        : IInteractionStrategy(controller)
    {
    }

protected:
    // ----------------------------------------------------------------
    //  视图锁定
    // ----------------------------------------------------------------

    /**
     * @brief 尝试锁定到指定视图
     *
     * 若当前未锁定，或已锁定到同一视图，则成功并返回 true；
     * 若已锁定到其他视图，则返回 false（本次事件应被忽略）。
     */
    bool TryLockView(int viewIndex) {
        if (m_activeViewIndex != -1 && m_activeViewIndex != viewIndex)
            return false;
        m_activeViewIndex = viewIndex;
        return true;
    }

    /// @brief 释放视图锁定（测量完成或取消后调用）
    void UnlockView() {
        m_activeViewIndex = -1;
    }

    // ----------------------------------------------------------------
    //  图像边界检查
    // ----------------------------------------------------------------

    /**
     * @brief 判断屏幕坐标对应的世界坐标是否在图像范围内
     * @return 在范围内返回 true
     */
    bool IsInsideImage(int viewIndex, int screenX, int screenY) const;

    // ----------------------------------------------------------------
    //  共享状态
    // ----------------------------------------------------------------

    /// 当前正在操作的视图索引；-1 表示未锁定
    int m_activeViewIndex = -1;

    /// 是否处于"编辑已有测量端点"状态（Ctrl 按下时激活）
    bool m_isEditingExisting = false;

    /// 正在编辑的测量 ID（-1 表示无）
    int m_editingMeasurementId = -1;

    /// 第一个点的世界坐标
    std::array<double, 3> m_firstWorldPos = { 0.0, 0.0, 0.0 };
};
