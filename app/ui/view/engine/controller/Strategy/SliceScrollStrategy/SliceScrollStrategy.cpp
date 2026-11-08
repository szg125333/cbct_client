#include "SliceScrollStrategy.h"
#include "interface/IViewController.h"

SliceScrollStrategy::SliceScrollStrategy(IViewController* controller)
    : IInteractionStrategy(controller)
{
}

void SliceScrollStrategy::HandleEvent(EventType type, int viewIndex, const EventData& data) {
    if (!m_controller) return;

    switch (type) {
    case EventType::LeftPress: {
        m_dragging = true;
        m_activeView = viewIndex;
        m_lastY = data.mousePosY;
        m_accumDelta = 0;
        break;
    }

    case EventType::LeftMove: {
        if (!m_dragging || viewIndex != m_activeView) return;

        int dy = data.mousePosY - m_lastY;
        m_lastY = data.mousePosY;

        m_accumDelta += dy;

        // 计算需要跳几个切片
        int sliceDelta = m_accumDelta / m_sensitivity;

        if (sliceDelta != 0) {
            // 一次性跳 N 个切片，而不是循环调用 N 次
            m_controller->ChangeSlice(viewIndex, sliceDelta);
            m_accumDelta -= sliceDelta * m_sensitivity;
        }
        break;
    }

    case EventType::LeftRelease: {
        m_dragging = false;
        m_activeView = -1;
        m_accumDelta = 0;
        break;
    }

    default:
        break;
    }
}

void SliceScrollStrategy::Clear(int viewIndex) {
    // 无需清理
    m_dragging = false;
    m_activeView = -1;
}