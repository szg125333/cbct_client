/**
 * @file acquisition_panel.h
 * @brief 左侧「采集启动」页：配方选择、设备状态、患者占位、开始按钮、检测记录表
 */
#ifndef ACQUISITION_PANEL_H
#define ACQUISITION_PANEL_H

#include "db/detection_record.h"
#include "ois/ois_types.h"
#include "recipe/recipe_params.h"

#include <QWidget>

class RecipeBridge;
class DetectorBridge;
class RecordBridge;
class OisBridge;

namespace Ui {
class AcquisitionPanel;
}

/**
 * @class AcquisitionPanel
 * @brief 挂在 LeftPanel 的 QStackedWidget 第 0 页
 *
 * 检测记录：启动时从 SQLite 读入表格；采集结束后写库再刷新。
 */
class AcquisitionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AcquisitionPanel(QWidget *parent = nullptr);
    ~AcquisitionPanel() override;

    void stopAcquisition();
    DetectorBridge *detectorBridge() const { return m_detectorBridge; }

signals:
    void acquisitionStarted();
    void acquisitionFinished(int totalFrames, const QString &recipeName);

private slots:
    void onRecipeIndexChanged(int index);
    void setupRecipeCombo();
    void onStartClicked();
    void onStopClicked();
    void onDownloadClicked();
    void onOisStudiesReady(const QList<cbct::OisStudy> &studies);
    void onOisDownloadDone(const cbct::OisDownloadResult &result);
    void onOisProgress(int framesDone, int framesTotal, qint64 bytes);
    void onOisFailed(const QString &message);

private:
    void setupRecordsTable();
    void updateSummaryFromSelection();
    bool currentRecipe(cbct::Recipe &out) const;

    /// 数据库失败统一提示（初始化/查询/写入）
    void showDatabaseError(const QString &message);
    /// 启动时检查数据库是否 ready，失败则明确提示
    void checkDatabaseOnStartup();

    /// 构造一条待入库记录
    cbct::DetectionRecord makeRecord(const QString &status, const QString &note, int frames) const;
    /// 写入 SQLite，成功后由 recordsUpdated 刷新表格
    void persistAndRefresh(const cbct::DetectionRecord &record);
    /// 用库中列表重填 tableRecords
    void fillRecordsTable(const QList<cbct::DetectionRecord> &list);

    Ui::AcquisitionPanel *ui;
    RecipeBridge *m_recipeBridge = nullptr;
    DetectorBridge *m_detectorBridge = nullptr;
    RecordBridge *m_recordBridge = nullptr; ///< 检测记录读写桥
    OisBridge *m_oisBridge = nullptr;       ///< OIS 服务器桥接
    QString m_currentRecipeName;
    QString m_currentRecipeId;              ///< 当前配方 id（入库用）

    /// 下载流程缓存：查询到的检查/序列，用于后续 get_ct_image
    QList<cbct::OisStudy> m_oisStudies;
    QList<cbct::OisSeries> m_oisSeries;
    QString m_downloadPatientId;
    QString m_downloadStudyUid;
    QString m_downloadSeriesUid;
    bool m_downloadPending = false;         ///< true=正在下载流程中
};

#endif // ACQUISITION_PANEL_H
