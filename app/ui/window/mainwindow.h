/**
 * @file mainwindow.h
 * @brief 主窗口：菜单栏 + 工具栏 + 左侧功能栏 + 右侧三视图/采集 2D 栈
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │ menubar：文件 | 编辑 | 视图 | 窗口 | 帮助                 │
 * ├─────────────────────────────────────────────────────────┤
 * │ mainToolbar：图标工具栏（视图/布局/测量等快捷入口）        │
 * ├──────────────┬──────────────────────────────────────────┤
 * │ LeftPanel    │ rightStack                                │
 * │ （功能侧栏） │  [0] ThreeViewPanel  三视图               │
 * │ stack 多页   │  [1] AcquisitionImagePanel  采集 2D      │
 * │ 第0页=采集   │                                          │
 * ├──────────────┴──────────────────────────────────────────┤
 * │ statusbar：状态与短暂提示                                 │
 * └─────────────────────────────────────────────────────────┘
 *
 * 菜单（二级 QAction 已挂接，见 mainwindow.ui）：
 *   文件 menuFile   —— 打开 DICOM 目录/文件、退出
 *   编辑 menuEdit   —— 长度/角度/矩形 ROI/手绘 ROI 模式、清除标注
 *   视图 menuView   —— 三视图↔采集切换、重置视图、普通/平移/缩放/探测/十字线
 *   窗口 menuWindow —— 左侧栏显隐
 *   帮助 menuHelp   —— 关于
 *
 * 模式互斥：菜单 QAction + 工具栏 checkable 按钮 → ModeBinding 表 →
 * applyMode() 单入口 → ThreeViewPanel::setInteractionMode。
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "common/InteractionMode.h"

#include <QMainWindow>

#include <functional>

class DicomBridge;

/// 模式互斥组绑定：工具栏 objectName / 菜单 QAction → InteractionMode
struct ModeBinding {
    const char *toolbarObject;  ///< nullptr = 无工具栏按钮
    const char *menuAction;     ///< nullptr = 无菜单项
    InteractionMode mode;
    bool hintOnly = false;      ///< true：仅提示（如缩放 Ctrl+左键），不切模式
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief 组装各面板；右侧 rightStack 在「三视图」与「采集 2D」间切换
 *
 * ui 关键 objectName：
 *   menubar, statusbar, mainToolbar
 *   leftPanel, mainSplitterLine, rightStack
 *   threeViewPanel, acquisitionImagePanel
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    /// 视图菜单/采集流程：右侧切到三视图页
    void showThreeView();

    /// 采集启动：右侧切到 2D 投影页并清空画面
    void showAcquisitionView();

protected:
    /// 点叉关闭：接受并退出整个应用（避免残留线程导致进程不退）
    void closeEvent(QCloseEvent *event) override;

private slots:
    /// 文件菜单/工具栏 btnOpen：QFileDialog 打开 DICOM 目录
    void openDicomDirectory();

    /// 文件菜单：QFileDialog 打开一个/多个 .dcm
    void openDicomFiles();

    /// 文件菜单：QFileDialog 打开 RTSTRUCT 轮廓文件
    void openRtStruct();

    /// 工具栏按钮业务路由（模式表命中 → applyMode；打开/重置单独处理）
    void onToolbarButton(const QString &objectName, bool checked);

private:
    /// 模式互斥单一入口：菜单 + 工具栏 + 初始模式都走这里
    void applyMode(const ModeBinding &binding, bool checked);

    /// 同步加载 + 进度框（loadFn 返回是否成功）
    void loadWithProgress(const std::function<bool()> &loadFn);

    /// 导出测量数据（btnSave）为 CSV
    void exportMeasurements();

    Ui::MainWindow *ui;
    DicomBridge *m_dicomBridge = nullptr;
    class ContourListPopup *m_contourPopup = nullptr;
};

#endif // MAINWINDOW_H
