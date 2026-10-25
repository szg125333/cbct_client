#include "acquisition_image_panel.h"
#include "ui_acquisition_image_panel.h"

#include "bridge/detector_bridge.h"
#include "common/log_service.h"
#include "device/frame_mailbox.h"

#include <QPixmap>
#include <QPushButton>
#include <QTimer>

AcquisitionImagePanel::AcquisitionImagePanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionImagePanel)
{
    ui->setupUi(this);

    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setMinimumSize(320, 240);
    ui->imageLabel->setStyleSheet(QStringLiteral("background:#111; color:#888;"));

    ui->progressBar->setRange(0, 1000);
    ui->progressBar->setValue(0);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(33); // UI 侧约 30fps 取帧
    connect(m_pollTimer, &QTimer::timeout, this, &AcquisitionImagePanel::onPollTimer);

    connect(ui->btnStop, &QPushButton::clicked,
            this, &AcquisitionImagePanel::stopRequested);
}

AcquisitionImagePanel::~AcquisitionImagePanel()
{
    stopPolling();
    delete ui;
}

void AcquisitionImagePanel::setDetectorBridge(DetectorBridge *bridge)
{
    m_bridge = bridge;
}

void AcquisitionImagePanel::startPolling()
{
    CBCT_LOG_DEBUG("ui", "2D 采集页开始拉帧");
    m_pollTimer->start();
}

void AcquisitionImagePanel::stopPolling()
{
    if (m_pollTimer->isActive()) {
        CBCT_LOG_DEBUG("ui", "2D 采集页停止拉帧");
    }
    m_pollTimer->stop();
}

void AcquisitionImagePanel::onPollTimer()
{
    if (!m_bridge) {
        return;
    }
    cbct::ProjectionFrame frame;
    if (m_bridge->tryPopFrame(frame)) {
        showFrame(frame.image, frame.index, frame.total, frame.angleDeg);
    }
}

void AcquisitionImagePanel::showFrame(const QImage &frame, int index, int total, double angleDeg)
{
    const QPixmap pm = QPixmap::fromImage(frame);
    const QSize target = ui->imageLabel->size();
    if (target.width() > 8 && target.height() > 8) {
        ui->imageLabel->setPixmap(pm.scaled(target, Qt::KeepAspectRatio, Qt::FastTransformation));
    } else {
        ui->imageLabel->setPixmap(pm);
    }

    ui->lbFrame->setText(
        tr("帧 %1 / %2    角度 %3°").arg(index + 1).arg(total).arg(angleDeg, 0, 'f', 2));

    if (total > 0) {
        ui->progressBar->setValue((index + 1) * 1000 / total);
    }
}

void AcquisitionImagePanel::onAcquisitionFinished()
{
    CBCT_LOG_INFO("ui", "采集完成，抽干帧队列");
    if (m_bridge) {
        cbct::ProjectionFrame frame;
        while (m_bridge->tryPopFrame(frame)) {
            showFrame(frame.image, frame.index, frame.total, frame.angleDeg);
        }
    }
    ui->lbFrame->setText(tr("采集完成"));
    ui->progressBar->setValue(1000);
}

void AcquisitionImagePanel::onAcquisitionError(const QString &message)
{
    CBCT_LOG_ERROR("ui", "采集显示错误: {}", message.toStdString());
    ui->lbFrame->setText(tr("错误: %1").arg(message));
}

void AcquisitionImagePanel::resetDisplay()
{
    ui->imageLabel->clear();
    ui->imageLabel->setText(tr("等待采集…"));
    ui->lbFrame->setText(tr("帧 — / —"));
    ui->progressBar->setValue(0);
}
