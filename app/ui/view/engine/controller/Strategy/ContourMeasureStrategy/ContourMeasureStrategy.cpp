#include "ContourMeasureStrategy.h"
#include <QDebug>

ContourMeasureStrategy::ContourMeasureStrategy(IViewController* controller)
    : IInteractionStrategy(controller) {
}

void ContourMeasureStrategy::HandleEvent(EventType type, int viewIndex, const EventData& data) {
    qDebug() << "[ContourMeasureStrategy] Event:" << static_cast<int>(type)
        << "ViewIndex:" << viewIndex;
}

void ContourMeasureStrategy::Clear(int viewIndex)
{
}
