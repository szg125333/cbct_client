#include "device/frame_mailbox.h"

namespace cbct {

FrameMailbox::FrameMailbox(int capacity)
    : m_capacity(capacity > 0 ? capacity : 1)
{
}

void FrameMailbox::push(ProjectionFrame frame)
{
    QMutexLocker lock(&m_mutex);
    // 背压：队列满则睡，直到消费者 pop 或 abort
    while (static_cast<int>(m_queue.size()) >= m_capacity && !m_aborted) {
        m_notFull.wait(&m_mutex);
    }
    if (m_aborted) {
        return; // 停止中：不再入队，避免采集线程卡死
    }
    m_queue.push_back(std::move(frame)); // 移动语义，避免 QImage 深拷贝
    m_notEmpty.wakeOne();
}

bool FrameMailbox::tryPop(ProjectionFrame &out)
{
    QMutexLocker lock(&m_mutex);
    if (m_queue.empty()) {
        return false; // 没有可显示的帧
    }
    out = std::move(m_queue.front());
    m_queue.pop_front();
    m_notFull.wakeOne(); // 腾出空位，唤醒可能阻塞的生产者
    return true;
}

void FrameMailbox::clear()
{
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
    m_notFull.wakeAll();
}

void FrameMailbox::abort()
{
    QMutexLocker lock(&m_mutex);
    m_aborted = true;
    m_queue.clear();
    // 唤醒所有可能卡在 push 上的等待
    m_notFull.wakeAll();
    m_notEmpty.wakeAll();
}

void FrameMailbox::reopen()
{
    QMutexLocker lock(&m_mutex);
    m_aborted = false;
    m_queue.clear();
}

int FrameMailbox::size() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_queue.size());
}

} // namespace cbct
