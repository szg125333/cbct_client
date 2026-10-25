#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "main_toolbar.h"
#include "left_panel.h"
#include "acquisition_panel.h"
#include "acquisition_image_panel.h"
#include "bridge/detector_bridge.h"
#include "common/log_service.h"

#include <QApplication>
#include <QCloseEvent>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    CBCT_LOG_INFO("ui", "主窗口初始化");

    // ---- 工具栏 → 状态栏 ----
    // 菜单/工具栏点击统一用 statusBar 短暂反馈；真正业务以后挂 QAction
    connect(ui->mainToolbar, &MainToolbar::actionTriggered,
            this, [this](const QString &actionName) {
                CBCT_LOG_DEBUG("ui", "工具栏点击: {}", actionName.toStdString());
                statusBar()->showMessage(actionName, 3000);
            });

    // ---- 左侧功能栏 ↔ 采集页 / 右侧 2D 页 ----
    // LeftPanel 第 0 页是 AcquisitionPanel（配方、开始采集、记录表）
    if (auto *panel = ui->leftPanel->acquisitionPanel()) {
        // 采集启动成功：右侧 rightStack 切到 AcquisitionImagePanel
        connect(panel, &AcquisitionPanel::acquisitionStarted,
                this, &MainWindow::showAcquisitionView);

        // 采集结束：状态栏提示，并切回三视图（视图菜单将来也可手动切）
        connect(panel, &AcquisitionPanel::acquisitionFinished,
                this, [this](int total, const QString &name) {
                    statusBar()->showMessage(
                        tr("采集完成：%1（%2 帧）").arg(name).arg(total), 5000);
                    showThreeView();
                });

        // 帧队列（FrameMailbox）→ 2D 页定时 pull 显示
        if (auto *bridge = panel->detectorBridge()) {
            ui->acquisitionImagePanel->setDetectorBridge(bridge);

            // 先 showAcquisitionView 再 startPolling，保证有界面再取帧
            connect(panel, &AcquisitionPanel::acquisitionStarted,
                    ui->acquisitionImagePanel, &AcquisitionImagePanel::startPolling);

            connect(bridge, &DetectorBridge::acquisitionFinished,
                    ui->acquisitionImagePanel, &AcquisitionImagePanel::onAcquisitionFinished);
            connect(bridge, &DetectorBridge::acquisitionFinished,
                    ui->acquisitionImagePanel, &AcquisitionImagePanel::stopPolling);

            connect(bridge, &DetectorBridge::acquisitionError,
                    ui->acquisitionImagePanel, &AcquisitionImagePanel::onAcquisitionError);
            connect(bridge, &DetectorBridge::acquisitionError,
                    ui->acquisitionImagePanel, &AcquisitionImagePanel::stopPolling);

            connect(bridge, &DetectorBridge::stopped,
                    ui->acquisitionImagePanel, &AcquisitionImagePanel::stopPolling);
        }
    }

    // 2D 页「停止」按钮 → 通知左侧 AcquisitionPanel 停采集
    connect(ui->acquisitionImagePanel, &AcquisitionImagePanel::stopRequested,
            this, [this]() {
                if (auto *panel = ui->leftPanel->acquisitionPanel()) {
                    panel->stopAcquisition();
                }
            });

    // 启动默认显示三视图；采集时由 showAcquisitionView 切换
    showThreeView();

    // 启动即最大化
    showMaximized();
}

MainWindow::~MainWindow()
{
    // VMware/GLEW 异常环境下，VTK 三视图析构可能卡住，导致点叉后进程不退。
    // 退出路径：先把三视图从窗口树摘掉并故意不 delete（进程退出由 OS 回收）。
    if (ui && ui->threeViewPanel) {
        ui->threeViewPanel->setParent(nullptr);
        ui->threeViewPanel->hide();
    }
    delete ui;
    ui = nullptr;
}

void MainWindow::showThreeView()
{
    CBCT_LOG_DEBUG("ui", "右侧切换到三视图");
    ui->rightStack->setCurrentWidget(ui->threeViewPanel);
}

void MainWindow::showAcquisitionView()
{
    CBCT_LOG_DEBUG("ui", "右侧切换到 2D 采集页");
    ui->acquisitionImagePanel->resetDisplay();
    ui->rightStack->setCurrentWidget(ui->acquisitionImagePanel);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    CBCT_LOG_INFO("ui", "主窗口关闭");
    // 显式退出应用，确保 aboutToQuit → ClientCore::shutdown 一定执行
    QMainWindow::closeEvent(event);
    QApplication::quit();
}
