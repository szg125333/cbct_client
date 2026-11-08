#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "main_toolbar.h"
#include "left_panel.h"
#include "contour_list_popup.h"
#include "acquisition_panel.h"
#include "acquisition_image_panel.h"
#include "bridge/detector_bridge.h"
#include "bridge/dicom_bridge.h"
#include "common/log_service.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStatusBar>
#include <QTextStream>
#include <QToolButton>

#include <array>
#include <cstring>

namespace {

// S6/S7 共用一张表，工具栏与菜单都从这里查
const std::array<ModeBinding, 9> kModeBindings = {{
    {"btnWindowLevel", "actionModeNormal", InteractionMode::Normal, false},
    {"btnPan", "actionModePan", InteractionMode::ImageDrag, false},
    {"btnZoom", "actionModeZoomHint", InteractionMode::Normal, true},
    {"btnPixelProbe", "actionModeProbe", InteractionMode::Normal, true},
    {"btnMoveCrosshair", "actionModeCrosshair", InteractionMode::CrosshairRuler, false},
    {"btnMeasureRuler", "actionModeDistance", InteractionMode::DistanceMeasure, false},
    {"btnAngleMeasure", "actionModeAngle", InteractionMode::AngleMeasure, false},
    {"btnPolygonSelect", "actionModeRectRoi", InteractionMode::RegistrationROI, false},
    {"btnDrawContour", "actionModeFreehandRoi", InteractionMode::FreehandROI, false},
}};

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dicomBridge(new DicomBridge(this))
{
    ui->setupUi(this);
    CBCT_LOG_INFO("ui", "主窗口初始化");

    // ---- 工具栏：业务按 objectName 路由；中文名仍进状态栏做反馈 ----
    connect(ui->mainToolbar, &MainToolbar::buttonClicked,
            this, &MainWindow::onToolbarButton);
    connect(ui->mainToolbar, &MainToolbar::actionTriggered,
            this, [this](const QString &actionName) {
                CBCT_LOG_DEBUG("ui", "工具栏点击: {}", actionName.toStdString());
                statusBar()->showMessage(actionName, 3000);
            });

    // ---- DICOM 加载 → 三视图 ----
    connect(m_dicomBridge, &DicomBridge::volumeLoaded, this, [this]() {
        ui->threeViewPanel->setDicomVolume(m_dicomBridge->image());
        ui->leftPanel->setDicomMetadata(m_dicomBridge->metadata());
        showThreeView();
        statusBar()->showMessage(m_dicomBridge->summary(), 5000);
    });
    connect(m_dicomBridge, &DicomBridge::loadFailed, this,
            [this](const QString &err) {
                QMessageBox::warning(this, tr("打开失败"),
                                     tr("未找到可显示的 DICOM 序列：%1").arg(err));
            });
    // RTSTRUCT 轮廓 → 三视图叠加
    connect(m_dicomBridge, &DicomBridge::rtStructLoaded, this,
            [this](std::shared_ptr<RTStructureData> data) {
                ui->threeViewPanel->setRTStructureData(std::move(data));
                statusBar()->showMessage(tr("RTSTRUCT 轮廓已加载"), 5000);
            });

    // ---- 菜单：文件 ----
    connect(ui->actionOpenDicomDir, &QAction::triggered,
            this, &MainWindow::openDicomDirectory);
    connect(ui->actionOpenDicomFiles, &QAction::triggered,
            this, &MainWindow::openDicomFiles);
    connect(ui->actionOpenRtStruct, &QAction::triggered,
            this, &MainWindow::openRtStruct);
    connect(ui->actionOpenSecondary, &QAction::triggered, this, [this]() {
        // 叠加图像（CBCT 等）同样走文件打开，不用默认路径
        const QStringList files = QFileDialog::getOpenFileNames(
            this, tr("打开叠加图像"), QString(),
            tr("DICOM 文件 (*.dcm *.DCM);;所有文件 (*)"));
        if (files.isEmpty()) return;
        if (!m_dicomBridge->loadSecondaryFiles(files)) {
            QMessageBox::warning(this, tr("打开失败"),
                                 tr("叠加图加载失败：%1").arg(m_dicomBridge->lastError()));
        }
    });
    connect(m_dicomBridge, &DicomBridge::secondaryLoaded, this,
            [this](vtkImageData *img) {
                ui->threeViewPanel->setSecondaryImage(img);
                statusBar()->showMessage(tr("叠加图像已加载"), 5000);
            });
    connect(ui->actionBlendRatio, &QAction::triggered, this, [this]() {
        bool ok = false;
        const double v = QInputDialog::getDouble(
            this, tr("混合比例"),
            tr("0 = 纯主图（CT），1 = 纯叠加图（CBCT）："),
            ui->threeViewPanel->blendRatio(), 0.0, 1.0, 2, &ok);
        if (ok) {
            ui->threeViewPanel->setBlendRatio(v);
        }
    });
    // 测量导出：btnSave
    connect(ui->mainToolbar, &MainToolbar::buttonClicked, this,
            [this](const QString &objectName, bool) {
                if (objectName != QLatin1String("btnSave")) return;
                exportMeasurements();
            });
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    // ---- 菜单：视图 ----
    connect(ui->actionShowThreeView, &QAction::triggered,
            this, &MainWindow::showThreeView);
    connect(ui->actionShowAcquisition, &QAction::triggered,
            this, &MainWindow::showAcquisitionView);
    connect(ui->actionResetViews, &QAction::triggered, this, [this]() {
        ui->threeViewPanel->resetViews();
    });
    connect(ui->actionContourList, &QAction::triggered, this, [this]() {
        if (!m_contourPopup) {
            m_contourPopup = new ContourListPopup(this);
            connect(m_contourPopup, &ContourListPopup::roiVisibilityChanged,
                    ui->threeViewPanel, &ThreeViewPanel::setRoiVisible);
            connect(m_contourPopup, &ContourListPopup::allVisibilityChanged, this,
                    [this](bool visible) {
                        for (const auto &roi : ui->threeViewPanel->roiList()) {
                            ui->threeViewPanel->setRoiVisible(roi.roiNumber, visible);
                        }
                    });
        }
        m_contourPopup->setRoiList(ui->threeViewPanel->roiList());
        m_contourPopup->show();
        m_contourPopup->raise();
        m_contourPopup->setFocus();
    });
    connect(ui->actionClearMeasurements, &QAction::triggered, this, [this]() {
        ui->threeViewPanel->clearMeasurements();
    });

    // ---- 菜单：窗口/帮助 ----
    connect(ui->actionToggleLeftPanel, &QAction::triggered, this,
            [this](bool checked) { ui->leftPanel->setVisible(checked); });
    connect(ui->actionAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("关于"),
                           tr("CBCT Client\n医学 CBCT 采集/重建/查看\n"
                              "DICOM 查看引擎移植自 DicomImageStudy"));
    });

    // ---- 模式互斥（菜单侧；工具栏在 onToolbarButton 内汇入同一入口）----
    // 用 triggered(bool) 带上选中态，取消选中时清掉所有绘制（对齐原版）
    for (const auto &b : kModeBindings) {
        if (!b.menuAction) continue;
        auto *act = findChild<QAction *>(b.menuAction);
        if (!act) continue;
        connect(act, &QAction::triggered, this,
                [this, b](bool checked) { applyMode(b, checked); });
    }
    // 模式类工具栏按钮设为 checkable（互斥由 applyMode 统一回写）
    for (const auto &b : kModeBindings) {
        if (!b.toolbarObject || b.hintOnly) continue;
        const auto btns = ui->mainToolbar->findChildren<QToolButton *>();
        for (auto *btn : btns) {
            if (btn->objectName() == QLatin1String(b.toolbarObject)) {
                btn->setCheckable(true);
            }
        }
    }

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

    // 初始模式 = 普通（窗宽窗位）
    applyMode(kModeBindings.front(), true);

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

void MainWindow::openDicomDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("打开 DICOM 目录"), QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    // 先枚举序列：多个则让用户选（序列切换，不臆造二级菜单）
    const auto series = m_dicomBridge->enumerateSeries(dir);
    if (series.isEmpty()) {
        QMessageBox::warning(this, tr("打开失败"), tr("目录内无可显示的 DICOM 序列"));
        return;
    }

    int picked = 0;
    if (series.size() > 1) {
        QStringList items;
        items.reserve(series.size());
        for (const auto &s : series) {
            const QString desc = s.description.isEmpty() ? s.seriesUID : s.description;
            items << QStringLiteral("%1 · %2 · %3 张").arg(s.modality, desc).arg(s.fileCount);
        }
        bool ok = false;
        const QString chosen = QInputDialog::getItem(
            this, tr("选择序列"),
            tr("目录内有 %1 个序列：").arg(series.size()),
            items, 0, false, &ok);
        if (!ok) return;
        picked = items.indexOf(chosen);
        if (picked < 0) picked = 0;
    }

    loadWithProgress([this, &dir, picked]() {
        return m_dicomBridge->loadSeriesAt(dir, picked);
    });
}

void MainWindow::openDicomFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("打开 DICOM 文件"), QString(),
        tr("DICOM 文件 (*.dcm *.DCM);;所有文件 (*)"));
    if (files.isEmpty()) return;

    loadWithProgress([this, &files]() { return m_dicomBridge->loadFiles(files); });
}

void MainWindow::openRtStruct()
{
    // 按规则：图像/轮廓获取一律走文件打开，不用默认路径
    const QString file = QFileDialog::getOpenFileName(
        this, tr("打开 RTSTRUCT 轮廓"), QString(),
        tr("RTSTRUCT 文件 (*.dcm *.DCM);;所有文件 (*)"));
    if (file.isEmpty()) return;

    if (!m_dicomBridge->loadRtStruct(file)) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("RTSTRUCT 解析失败：%1").arg(m_dicomBridge->lastError()));
    }
}

void MainWindow::exportMeasurements()
{
    const auto records = ui->threeViewPanel->measurementRecords();
    if (records.empty()) {
        statusBar()->showMessage(tr("没有可导出的测量"), 4000);
        return;
    }

    const QString file = QFileDialog::getSaveFileName(
        this, tr("导出测量数据"), QStringLiteral("measurements.csv"),
        tr("CSV 文件 (*.csv);;所有文件 (*)"));
    if (file.isEmpty()) return;

    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件：%1").arg(file));
        return;
    }
    QTextStream out(&f);
    out << "type,id,view,value,unit,p0x,p0y,p0z,p1x,p1y,p1z,p2x,p2y,p2z,complete\n";
    const char *viewNames[] = {"axial", "sagittal", "coronal"};
    for (const auto &r : records) {
        out << (r.kind == MeasurementRecord::Kind::Distance ? "distance" : "angle")
            << ',' << r.id << ','
            << (r.viewIndex >= 0 && r.viewIndex < 3 ? viewNames[r.viewIndex] : "?")
            << ',' << QString::number(r.value, 'f', 3) << ','
            << (r.kind == MeasurementRecord::Kind::Distance ? "mm" : "deg");
        for (int p = 0; p < 3; ++p) {
            if (p < r.pointCount) {
                out << ',' << QString::number(r.points[p][0], 'f', 3)
                    << ',' << QString::number(r.points[p][1], 'f', 3)
                    << ',' << QString::number(r.points[p][2], 'f', 3);
            } else {
                out << ",,,";
            }
        }
        out << ',' << (r.complete ? 1 : 0) << '\n';
    }
    f.close();
    statusBar()->showMessage(tr("已导出 %1 条测量 → %2").arg(records.size()).arg(file), 5000);
}

void MainWindow::loadWithProgress(const std::function<bool()> &loadFn)
{
    // 异步加载：进度框非阻塞显示，volumeLoaded/loadFailed 时关闭
    auto *progress = new QProgressDialog(tr("正在加载 DICOM…"), QString(), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    // 加载完成/失败 → 关进度框（只连一次）
    auto *bridge = m_dicomBridge;
    connect(bridge, &DicomBridge::volumeLoaded, progress, &QProgressDialog::close);
    connect(bridge, &DicomBridge::loadFailed, progress, &QProgressDialog::close);

    if (!loadFn()) {
        progress->close();
    }
}

void MainWindow::onToolbarButton(const QString &objectName, bool checked)
{
    // 模式表命中 → 统一 applyMode（带选中态：取消选中时清所有绘制）
    for (const auto &b : kModeBindings) {
        if (b.toolbarObject && objectName == QLatin1String(b.toolbarObject)) {
            applyMode(b, checked);
            return;
        }
    }

    // 文件打开：目录 / 文件二选一
    if (objectName == QLatin1String("btnOpen")) {
        const auto ret = QMessageBox::question(
            this, tr("打开 DICOM"),
            tr("选择「目录」打开整个序列，或选择「文件」打开单个/多个 .dcm？"),
            tr("目录"), tr("文件"), tr("取消"));
        if (ret == 0) {
            openDicomDirectory();
        } else if (ret == 1) {
            openDicomFiles();
        }
        return;
    }

    // 重置视图 / 回三视图
    if (objectName == QLatin1String("btnAxialView")) {
        showThreeView();
        ui->threeViewPanel->resetViews();
        return;
    }

    // 未接管按钮：仅状态栏反馈（actionTriggered 已显示中文名，这里记日志）
    CBCT_LOG_DEBUG("ui", "未接管工具栏按钮: {}", objectName.toStdString());
}

void MainWindow::applyMode(const ModeBinding &binding, bool checked)
{
    if (binding.hintOnly) {
        // 提示态：切回 Normal（计划：缩放=Normal 的 Ctrl+左键），并给文案提示
        ui->threeViewPanel->setInteractionMode(binding.mode);
        if (binding.toolbarObject &&
            std::strcmp(binding.toolbarObject, "btnZoom") == 0) {
            statusBar()->showMessage(tr("缩放：Ctrl + 左键拖拽"), 4000);
        } else {
            statusBar()->showMessage(
                tr("像素探测：悬停查看体素值（信息栏实时显示）"), 4000);
        }
    } else {
        CBCT_LOG_DEBUG("ui", "切换模式 mode={} checked={}",
                       static_cast<int>(binding.mode), checked);
        // 对齐原版：取消选中 → 清所有绘制并回 Normal；选中 → 清干净后进入该模式
        ui->threeViewPanel->setModeToMeasurement(binding.mode, checked);
    }

    // 回写互斥 check：只勾当前绑定，取消其余（菜单 + 工具栏同步）
    // checked=false（取消选中）时全部不勾
    for (const auto &b : kModeBindings) {
        // 按绑定身份比较（lambda 捕获的是拷贝，不能比地址）
        const bool on =
            checked &&
            ((binding.menuAction && b.menuAction &&
              std::strcmp(b.menuAction, binding.menuAction) == 0) ||
             (binding.toolbarObject && b.toolbarObject &&
              std::strcmp(b.toolbarObject, binding.toolbarObject) == 0));
        if (b.menuAction) {
            if (auto *act = findChild<QAction *>(b.menuAction)) {
                act->setChecked(on);
            }
        }
        if (b.toolbarObject) {
            const auto btns = ui->mainToolbar->findChildren<QToolButton *>();
            for (auto *btn : btns) {
                if (btn->objectName() == QLatin1String(b.toolbarObject)) {
                    btn->setChecked(on);
                }
            }
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    CBCT_LOG_INFO("ui", "主窗口关闭");
    // 显式退出应用，确保 aboutToQuit → ClientCore::shutdown 一定执行
    QMainWindow::closeEvent(event);
    QApplication::quit();
}
