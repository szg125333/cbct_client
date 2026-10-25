#include "device/detector_simulator.h"

#include "common/log_service.h"

namespace cbct {

DetectorSimulator::DetectorSimulator(QObject *parent)
    : QObject(parent)
{
    // 定时器作为 this 的子对象：moveToThread 时一起到工作线程
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::CoarseTimer); // 不必高精度，30fps 级别足够
    connect(m_timer, &QTimer::timeout, this, &DetectorSimulator::onTick);
}

void DetectorSimulator::setFrameMailbox(FrameMailbox *mailbox)
{
    m_mailbox = mailbox;
}

int DetectorSimulator::frameCount() const
{
    return m_sequence.frameCount();
}

bool DetectorSimulator::beginAcquisition(const DetectorParams &params, const QString &projectionDir)
{
    m_lastBeginOk = false;
    m_lastError.clear();

    if (m_state == DetectorState::Running) {
        m_lastError = QStringLiteral("采集进行中");
        return false;
    }
    // 校验配方里的平板参数是否可用于本次模拟
    if (params.width <= 0 || params.height <= 0 || params.frameRateFps <= 0) {
        m_lastError = QStringLiteral("平板参数非法");
        return false;
    }

    setState(DetectorState::ApplyingParams);
    m_params = params;
    // 帧率 → 定时器间隔；例如 30fps → 33ms
    m_intervalMs = qMax(1, 1000 / params.frameRateFps);

    setState(DetectorState::Initializing);
    QString localError;
    if (!m_sequence.open(projectionDir, &localError)) {
        fail(localError);
        return false;
    }
    // 16bit 原始帧缓冲：宽*高 个 uint16，全程复用
    m_frameBuffer.resize(m_sequence.width() * m_sequence.height());
    m_nextIndex = 0;

    if (!m_mailbox) {
        fail(QStringLiteral("帧队列未配置"));
        return false;
    }
    m_mailbox->reopen(); // 复位 aborted，清空残留帧

    setState(DetectorState::Running);
    m_timer->start(m_intervalMs);
    m_lastBeginOk = true;
    CBCT_LOG_INFO("device", "开始采集 dir={} frames={} intervalMs={}",
                  projectionDir.toStdString(),
                  m_sequence.frameCount(),
                  m_intervalMs);
    return true;
}

void DetectorSimulator::endAcquisition()
{
    CBCT_LOG_INFO("device", "结束采集 state={} nextIndex={}",
                  static_cast<int>(m_state), m_nextIndex);
    m_timer->stop();
    if (m_mailbox) {
        m_mailbox->abort();
    }
    if (m_state == DetectorState::Running || m_state == DetectorState::Initializing) {
        setState(DetectorState::Idle);
    }
}

void DetectorSimulator::onTick()
{
    if (m_state != DetectorState::Running) {
        m_timer->stop();
        return;
    }
    if (!nextFrame()) {
        // 正常读完全部帧 → Finished；失败时 nextFrame 内已 fail()
        m_timer->stop();
        if (m_state == DetectorState::Running) {
            CBCT_LOG_INFO("device", "全部帧发送完成 total={}", m_sequence.frameCount());
            setState(DetectorState::Finished);
            emit finished();
        }
    }
}

void DetectorSimulator::setState(DetectorState s)
{
    if (m_state == s) {
        return;
    }
    CBCT_LOG_DEBUG("device", "状态 {} -> {}", static_cast<int>(m_state), static_cast<int>(s));
    m_state = s;
    emit stateChanged(static_cast<int>(s));
}

void DetectorSimulator::fail(const QString &message)
{
    m_lastError = message;
    CBCT_LOG_ERROR("device", message.toStdString());
    m_timer->stop();
    setState(DetectorState::Error);
    emit errorOccurred(message);
}

bool DetectorSimulator::nextFrame()
{
    const int total = m_sequence.frameCount();
    if (m_nextIndex >= total) {
        return false; // 没有更多帧
    }

    QString error;
    if (!m_sequence.readFrame(m_nextIndex, m_frameBuffer, &error)) {
        fail(error);
        return false;
    }

    // ---- 16bit → 8bit 显示用灰度 ----
    // lo/hi：本帧像素最小/最大值，用于线性拉伸到 0..255
    uint16_t lo = 0xFFFF;
    uint16_t hi = 0;
    for (uint16_t v : m_frameBuffer) {
        if (v < lo) {
            lo = v;
        }
        if (v > hi) {
            hi = v;
        }
    }
    const uint16_t span = (hi > lo) ? static_cast<uint16_t>(hi - lo) : 1;

    QImage image(m_sequence.width(), m_sequence.height(), QImage::Format_Grayscale8);
    uchar *bits = image.bits();
    const int stride = image.bytesPerLine(); // QImage 行可能有对齐填充
    const int w = m_sequence.width();
    const int h = m_sequence.height();
    for (int y = 0; y < h; ++y) {
        const uint16_t *row = m_frameBuffer.constData() + y * w;
        uchar *out = bits + y * stride;
        for (int x = 0; x < w; ++x) {
            out[x] = static_cast<uchar>(((row[x] - lo) * 255u) / span);
        }
    }

    ProjectionFrame frame;
    frame.image = std::move(image);
    frame.index = m_nextIndex;
    frame.total = total;
    frame.angleDeg = m_sequence.gantryAngleDeg(m_nextIndex);
    if (m_nextIndex == 0 || m_nextIndex == total - 1) {
        CBCT_LOG_DEBUG("device", "出帧 index={} total={} angle={:.2f}",
                       m_nextIndex, total, frame.angleDeg);
    }
    ++m_nextIndex;

    if (m_mailbox) {
        // 队列满时此处阻塞 → UI 跟得上才继续读下一帧（背压）
        m_mailbox->push(std::move(frame));
        if (m_state != DetectorState::Running) {
            return false; // push 期间被 endAcquisition 停掉
        }
    }
    return true;
}

} // namespace cbct
