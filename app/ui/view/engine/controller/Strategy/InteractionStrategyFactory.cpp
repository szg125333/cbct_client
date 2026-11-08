#include "InteractionStrategyFactory.h"

#include "NormalStrategy/NormalStrategy.h"
#include "DistanceMeasureStrategy/DistanceMeasureStrategy.h"
#include "RegistrationROIStrategy/RegistrationROIStrategy.h"
#include "CheckboardStrategy/CheckboardStrategy.h"
//#include "ManualMoveStrategy.h"
#include "ContourMeasureStrategy/ContourMeasureStrategy.h"
#include "FreehandROIStrategy/FreehandROIStrategy.h"    // 新增
#include "ImageDragStrategy/ImageDragStrategy.h"    // 新增
#include "RulerLineStrategy/RulerLineStrategy.h"
#include "AngleMeasureStrategy/AngleMeasureStrategy.h"
#include "SliceScrollStrategy/SliceScrollStrategy.h"

std::map<InteractionMode, std::unique_ptr<IInteractionStrategy>>
InteractionStrategyFactory::CreateStrategies(IViewController* controller)
{
    std::map<InteractionMode, std::unique_ptr<IInteractionStrategy>> strategies;

    strategies[InteractionMode::Normal]
        = std::make_unique<NormalStrategy>(controller);

    strategies[InteractionMode::DistanceMeasure]
        = std::make_unique<DistanceMeasureStrategy>(controller);

    strategies[InteractionMode::AngleMeasure]
        = std::make_unique<AngleMeasureStrategy>(controller);

    strategies[InteractionMode::RegistrationROI]
        = std::make_unique<RegistrationROIStrategy>(controller);

    strategies[InteractionMode::Checkboard]
        = std::make_unique<CheckboardStrategy>(controller);

    strategies[InteractionMode::ImageDrag]
        = std::make_unique<ImageDragStrategy>(controller);

    strategies[InteractionMode::ContourMeasure]
        = std::make_unique<ContourMeasureStrategy>(controller);

    strategies[InteractionMode::FreehandROI]
        = std::make_unique<FreehandROIStrategy>(controller);

    strategies[InteractionMode::CrosshairRuler]
        = std::make_unique<RulerLineStrategy>(controller);

    strategies[InteractionMode::SliceScroll]
        = std::make_unique<SliceScrollStrategy>(controller);

    return strategies;
}
