#pragma once
#include "../IInteractionStrategy.h"

class IViewController;

/// @brief 轮廓测量/手绘轮廓交互策略
/// 
/// 处理轮廓测量和手绘轮廓模式下的交互事件
class ContourMeasureStrategy : public IInteractionStrategy {
public:
    explicit ContourMeasureStrategy(IViewController* controller);
    ~ContourMeasureStrategy() override = default;

    void HandleEvent(EventType type, int viewIndex, const EventData& data) override;
    void Clear(int viewIndex) override;

};