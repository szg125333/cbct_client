#pragma once
#include "../IInteractionStrategy.h"

class IViewController;

/// @brief 棋盘格对比模式交互策略
/// 
/// 处理棋盘格对比模式下的交互事件
class CheckboardStrategy : public IInteractionStrategy {
public:
    explicit CheckboardStrategy(IViewController* controller);
    ~CheckboardStrategy() override = default;

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

};