/**
 * @file ois_bridge.h
 * @brief app ↔ core OIS 适配层：UI 不直接碰 OisService/QTcpSocket
 */
#ifndef OIS_BRIDGE_H
#define OIS_BRIDGE_H

#include "ois/ois_types.h"

#include <QObject>

/**
 * @class OisBridge
 * @brief 下载检查数据；结果以信号回 UI 线程
 */
class OisBridge : public QObject
{
    Q_OBJECT

public:
    explicit OisBridge(QObject *parent = nullptr);

    /// 查询服务器上的检查列表
    void fetchStudies();

    /// 下载指定检查/序列到本地 ois_downloads
    void download(const QString &patientId, const QString &studyUid, const QString &seriesUid);

    /// 上传预留（当前未实现）
    void upload(const QString &patientId, const QString &studyUid, const QString &seriesUid);

signals:
    void statusText(const QString &text);
    void studiesReady(const QList<cbct::OisStudy> &studies);
    void downloadProgress(int framesDone, int framesTotal, qint64 bytes);
    void downloadDone(const cbct::OisDownloadResult &result);
    void failed(const QString &message);
    void uploadNotReady(const QString &message);
};

#endif // OIS_BRIDGE_H
