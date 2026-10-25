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
 * 菜单规划（二级项尚未添加，详见 mainwindow.ui 内 XML 注释）：
 *   文件 menuFile   —— 患者/数据/导入导出/退出
 *   编辑 menuEdit   —— 撤销重做、复制粘贴、清除标注
 *   视图 menuView   —— 三视图↔采集切换、布局、窗宽窗位
 *   窗口 menuWindow —— 左侧栏显隐、重置布局（固定分栏，非 QDockWidget）
 *   帮助 menuHelp   —— 关于、版本、说明
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
