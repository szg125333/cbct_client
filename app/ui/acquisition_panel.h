/**
 * @file acquisition_panel.h
 * @brief 左侧「采集启动」页：配方选择、设备状态、患者占位、开始按钮、检测记录表
 */
#ifndef ACQUISITION_PANEL_H
#define ACQUISITION_PANEL_H

#include "recipe/recipe_params.h"

#include <QWidget>

class RecipeBridge;    ///< 配方列表适配（读 ClientCore）
class DetectorBridge;  ///< 采集启动/停止 + 帧队列访问

namespace Ui {
class AcquisitionPanel;
}

/**
 * @class AcquisitionPanel
 * @brief 挂在 LeftPanel 的 QStackedWidget 第 0 页
 */
class AcquisitionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AcquisitionPanel(QWidget *parent = nullptr);
    ~AcquisitionPanel() override;

    void stopAcquisition(); ///< 供主窗口「停止」按钮调用
    DetectorBridge *detectorBridge() const { return m_detectorBridge; } ///< 主窗口用它连帧队列

signals:
    /// 已成功 start：主窗口应切到右侧 2D 采集页
    void acquisitionStarted();
    /// 采集结束或失败回退：totalFrames=0 表示失败/中止
    void acquisitionFinished(int totalFrames, const QString &recipeName);

private slots:
    void onRecipeIndexChanged(int index); ///< 下拉变化；选「＋」则开新增对话框
    void setupRecipeCombo();              ///< 从 bridge 重填下拉框
    void onStartClicked();                ///< 开始采集
    void onStopClicked();

private:
    void setupRecordsTable();             ///< 配置检测记录表头
    void updateSummaryFromSelection();    ///< 更新 kV/mA/时间摘要标签
    void appendRecord(const QString &status, const QString &note); ///< 记录表加一行
    bool currentRecipe(cbct::Recipe &out) const; ///< 取当前选中配方

    Ui::AcquisitionPanel *ui;
    RecipeBridge *m_recipeBridge = nullptr;   ///< 配方数据桥
    DetectorBridge *m_detectorBridge = nullptr; ///< 采集桥
    QString m_currentRecipeName;              ///< 本页最近一次启动用的配方名（记录表用）
};

#endif // ACQUISITION_PANEL_H
