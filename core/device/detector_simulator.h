/**
 * @file detector_simulator.h
 * @brief 模拟平板采集器：在工作线程按帧读 HIS，推入 FrameMailbox
 *
 * 职责边界：
 * - 不直接碰 UI；帧通过 FrameMailbox 异步交给界面
 * - 不落盘；只负责“像真平板一样”按节奏出帧
 * - 跨线程入口：beginAcquisition / endAcquisition 槽
 */
#ifndef CBCT_DETECTOR_SIMULATOR_H
#define CBCT_DETECTOR_SIMULATOR_H

#include "device/detector_types.h"
#include "device/frame_mailbox.h"
#include "device/his_projection_sequence.h"
#include "recipe/recipe_params.h"

#include <QObject>
#include <QTimer>
#include <QVector>
#include <cstdint>

namespace cbct {

/**
 * @class DetectorSimulator
 * @brief 平板采集模拟（对象应 moveToThread 到工作线程）
 *
 * 状态机（DetectorState）：
 *   Idle → ApplyingParams → Initializing → Running → Finished / Error
 */
class DetectorSimulator : public QObject
{
    Q_OBJECT

public:
    explicit DetectorSimulator(QObject *parent = nullptr);

    /// 绑定帧邮箱（共享给 UI 的有界队列）
    void setFrameMailbox(FrameMailbox *mailbox);
    FrameMailbox *frameMailbox() const { return m_mailbox; }

    DetectorState state() const { return m_state; }
    int frameCount() const;          ///< 已打开序列的总帧数
    QString lastError() const { return m_lastError; } ///< 最近一次失败原因
    bool lastBeginOk() const { return m_lastBeginOk; } ///< beginAcquisition 是否成功

public slots:
    /**
     * @brief 在工作线程执行：校验参数 → 打开序列 → 启动定时出帧
     * @param params 平板参数（来自当前配方）
     * @param projectionDir 投影数据目录（如 /mnt/hgfs/share/catphan）
     * @return 成功返回 true；失败原因写入 m_lastError
     */
    bool beginAcquisition(const cbct::DetectorParams &params, const QString &projectionDir);

    /// 停止定时器；abort 邮箱以解除可能阻塞的 push
    void endAcquisition();

signals:
    void stateChanged(int state);       ///< int 为 DetectorState，便于跨线程信号
    void finished();                    ///< 全部帧推完
    void errorOccurred(const QString &message);

private slots:
    void onTick(); ///< 定时器回调：产出下一帧

private:
    void setState(DetectorState s);
    void fail(const QString &message);
    bool nextFrame(); ///< 读一帧 HIS → 转 QImage → push 邮箱

    FrameMailbox *m_mailbox = nullptr;  ///< 帧出口；nullptr 时 begin 会失败
    DetectorState m_state = DetectorState::Idle; ///< 当前状态机状态
    DetectorParams m_params;            ///< 已下发的平板参数快照
    HisProjectionSequence m_sequence;   ///< 目录内 image_rawA_*.his 序列
    QVector<uint16_t> m_frameBuffer;    ///< 复用的 16bit 像素缓冲，避免每帧重分配
    QTimer *m_timer = nullptr;          ///< 出帧节拍；parent=this，随对象进工作线程
    int m_nextIndex = 0;                ///< 下一帧要读的序号
    int m_intervalMs = 33;              ///< 帧间隔 ms ≈ 1000/frameRateFps
    QString m_lastError;                ///< 最近错误文案
    bool m_lastBeginOk = false;         ///< 最近 begin 是否成功
};

} // namespace cbct

#endif // CBCT_DETECTOR_SIMULATOR_H
