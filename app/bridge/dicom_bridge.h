/**
 * @file dicom_bridge.h
 * @brief app ↔ core DICOM 加载适配层：GDCM 解析在 core，VTK 转换在 app
 *
 * 第一阶段同步加载（几百张 dcm < 1s），UI 配 QProgressDialog；
 * 若以后大目录卡 UI，可把 load* 改为 invokeMethod 到 core 工作线程。
 */
#ifndef DICOM_BRIDGE_H
#define DICOM_BRIDGE_H

#include "dicom/dicom_types.h"

#include <QObject>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

// 引擎侧纯数据结构（无 VTK），bridge 负责 core → 引擎类型转换
struct RTStructureData;

/**
 * @class DicomBridge
 * @brief 打开 DICOM 目录/文件 → DicomVolumeData → vtkImageData
 *
 * 体数据加载在 QtConcurrent 工作线程（core 解析同步 API 不变），
 * VTK 转换与信号发射回到主线程；加载期间 m_loading 挡重复请求。
 */
class DicomBridge : public QObject
{
    Q_OBJECT

public:
    explicit DicomBridge(QObject *parent = nullptr);

    /// 打开目录：自动选最大非 RT 序列（异步；结果走 volumeLoaded/loadFailed）
    bool loadDirectory(const QString &directory);

    /// 目录内的可成体序列摘要（供 UI 选择）
    struct SeriesSummary {
        QString seriesUID;
        QString modality;
        QString description;
        int fileCount = 0;
    };

    /// 枚举目录内序列（已过滤 RT 类）
    QVector<SeriesSummary> enumerateSeries(const QString &directory);

    /// 按 enumerateSeries 的下标加载该序列
    bool loadSeriesAt(const QString &directory, int index);

    /// 打开单个/多个 .dcm（多文件须同序列）
    bool loadFiles(const QStringList &files);

    /// 加载 RTSTRUCT 轮廓文件（core 解析 → 引擎 RTStructureData）
    /// 成功发 rtStructLoaded；失败走 loadFailed
    bool loadRtStruct(const QString &filePath);

    /// 加载叠加图像（CBCT 等，须与主图同网格）；成功发 secondaryLoaded
    bool loadSecondaryFiles(const QStringList &files);

    /// 当前体数据（内部持有，桥接层析构时释放）
    vtkImageData *image() const { return m_image; }

    /// 「163 张 · 512×512 · 0.977mm」给状态栏
    QString summary() const;

    /// 当前体数据的 DICOM 元数据（core 解析结果；UI 表格用）
    const std::map<std::string, std::string> &metadata() const { return m_volume.metadata; }

    /// 上次失败原因
    QString lastError() const { return m_lastError; }

signals:
    void volumeLoaded();
    void loadFailed(const QString &error);
    void rtStructLoaded(std::shared_ptr<RTStructureData> data);
    void secondaryLoaded(vtkImageData *image);
    void loadStarted();  ///< 工作线程加载开始（UI 显示进度用）

private:
    /// 体数据加载流程：工作线程跑 loader，主线程 adoptVolume
    void startLoad(std::function<cbct::dicom::DicomVolumeData(cbct::dicom::DicomLoadError *)> loader);

    bool adoptVolume(cbct::dicom::DicomVolumeData &&volume);
    static vtkImageData *toVtkImageData(const cbct::dicom::DicomVolumeData &volume);

    vtkSmartPointer<vtkImageData> m_image;
    cbct::dicom::DicomVolumeData m_volume;
    QString m_lastError;
    std::atomic<bool> m_loading{false};
};

#endif // DICOM_BRIDGE_H
