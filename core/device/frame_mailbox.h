/**
 * @file frame_mailbox.h
 * @brief 有界线程安全队列：采集线程（生产）→ UI 线程（消费）
 *
 * 模式：生产者-消费者 + 背压（back-pressure）
 * - 生产者 push：队列满则阻塞等待，避免内存无界增长
 * - 消费者 tryPop：队列空则立即返回 false（UI 定时来取，不阻塞）
 * - stop 时 abort()：唤醒阻塞中的 push，并拒绝继续入队
 */
#ifndef CBCT_FRAME_MAILBOX_H
#define CBCT_FRAME_MAILBOX_H

#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <deque>

namespace cbct {

/// @brief 一帧投影数据包（采集线程打包，UI 线程消费）
struct ProjectionFrame {
    QImage image;   ///< 已转好的 8bit 灰度图，UI 可直接转 QPixmap
    int index = -1; ///< 帧序号，从 0 起
    int total = 0;  ///< 本次采集总帧数（进度条用）
    double angleDeg = 0.0; ///< 该帧对应的龙门架角度（°）
};

/**
 * @class FrameMailbox
 * @brief 单生产者单消费者（SPSC）语义下的有界队列
 *
 * 内部：std::deque + QMutex + 两个 QWaitCondition
 * - m_notFull：队列满时生产者 wait；消费者 pop 后 wake
 * - m_notEmpty：预留（当前消费者是 tryPop 非阻塞，用不到阻塞等待）
 */
class FrameMailbox
{
public:
    /// @param capacity 队列最大帧数；<=0 时按 1 处理
    explicit FrameMailbox(int capacity = 16);

    /// 生产：满则阻塞；aborted 后丢弃并返回
    void push(ProjectionFrame frame);

    /// 消费：非阻塞；成功则把帧移入 out 并返回 true
    bool tryPop(ProjectionFrame &out);

    void clear();   ///< 清空队列（不改 aborted 标志逻辑以外状态）
    void abort();   ///< 停止采集：清空 + 唤醒所有阻塞 push
    void reopen();  ///< 下次采集开始前复位 aborted 并清空

    int size() const;      ///< 当前队列中帧数（调试/监控）
    int capacity() const;  ///< 容量上限

private:
    mutable QMutex m_mutex;          ///< 保护 m_queue / m_aborted
    QWaitCondition m_notFull;        ///< 队列满时生产者等待
    QWaitCondition m_notEmpty;       ///< 队列空时消费者可等待（当前未用阻塞 pop）
    std::deque<ProjectionFrame> m_queue; ///< 实际缓冲；deque 便于两端操作
    int m_capacity;                  ///< 容量；满则 push 阻塞
    bool m_aborted = false;          ///< true=停止中，push 直接丢弃
};

} // namespace cbct

#endif // CBCT_FRAME_MAILBOX_H
