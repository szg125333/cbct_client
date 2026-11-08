#pragma once

#include "interface/IViewRenderer.h"
#include "common/InteractionMode.h"
#include "common/EventData.h"

class IViewController;

/**
 * @brief 交互策略接口
 *
 * 所有用户交互模式（普通浏览、测距、测角、ROI 等）均继承此接口。
 * ThreeViewController 在当前模式对应的策略上调用 HandleEvent 和 Clear。
 */
class IInteractionStrategy {
public:
    explicit IInteractionStrategy(IViewController* controller)
        : m_controller(controller)
    {
    }

    virtual ~IInteractionStrategy() = default;

    /**
     * @brief 处理交互事件
     * @param type      事件类型（鼠标 / 键盘）
     * @param viewIndex 事件来源的视图索引（0–2）
     * @param data      事件附属数据（坐标、修饰键等）
     */
    virtual void HandleEvent(EventType type, int viewIndex, const EventData& data) = 0;

    /**
     * @brief 清除指定视图上本策略产生的所有 Overlay 标注
     * @param viewIndex 目标视图索引
     */
    virtual void Clear(int viewIndex) = 0;

    virtual void OnActivated() {}

protected:
    /// 关联的视图控制器，用于访问渲染器和图像数据
    IViewController* m_controller = nullptr;
};
