#pragma once

#include "common/InteractionMode.h"
#include <map>
#include <memory>

class IInteractionStrategy;
class IViewController;

/**
 * @brief 交互策略工厂
 *
 * 集中管理所有 InteractionMode 与对应 Strategy 的映射关系。
 * ThreeViewController 构造时调用 CreateStrategies() 一次性创建全部策略。
 */
class InteractionStrategyFactory {
public:
    /**
     * @brief 创建全套交互策略
     * @param controller 关联的视图控制器（所有策略均持有此指针）
     * @return 模式 → 策略 映射表
     */
    static std::map<InteractionMode, std::unique_ptr<IInteractionStrategy>>
        CreateStrategies(IViewController* controller);
};
