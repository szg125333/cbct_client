#ifndef ACQUISITION_IMAGE_PANEL_H
#define ACQUISITION_IMAGE_PANEL_H

#include <QImage>
#include <QWidget>

class DetectorBridge;
class QTimer;

namespace Ui {
class AcquisitionImagePanel;
}

/**
 * @class AcquisitionImagePanel
 * @brief 右侧 stack 页：2D 投影预览
 *
 * 消费模型：定时从 FrameMailbox tryPop，不必与采集严格同步；
 * 显示略慢时靠队列缓冲（见 FrameMailbox 背压）。
 */
class AcquisitionImagePanel : public QWidget
{
    Q_OBJECT

public:
    explicit AcquisitionImagePanel(QWidget *parent = nullptr);
    ~AcquisitionImagePanel() override;

    void setDetectorBridge(DetectorBridge *bridge); ///< 由 MainWindow 注入

public slots:
    void startPolling(); ///< 开始定时取帧
    void stopPolling();  ///< 停止取帧
    void onAcquisitionFinished();                 ///< 抽干队列剩余帧后显示完成
    void onAcquisitionError(const QString &message);
    void resetDisplay();                           ///< 切页时清空画面

signals:
    void stopRequested(); ///< 用户点了「停止」

private slots:
    void onPollTimer(); ///< 33ms：尝试弹一帧并显示

private:
    void showFrame(const QImage &frame, int index, int total, double angleDeg);

    Ui::AcquisitionImagePanel *ui;
    DetectorBridge *m_bridge = nullptr; ///< 帧队列入口（ClientCore::frameMailbox）
    QTimer *m_pollTimer = nullptr;      ///< UI 取帧定时器；空则不显示
};

#endif // ACQUISITION_IMAGE_PANEL_H
