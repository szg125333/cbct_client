/**
 * @file ois_service.h
 * @brief OIS 影像服务器客户端（core 内，不依赖 Widgets）
 *
 * 协议对齐 /home/szg/code/ois_server（event 处理 PROTO_CUSTOM）：
 * - 请求：一行 Compact JSON + '\n'（服务端 readline）
 * - 响应：4 字节网络序长度 + JSON 文本
 * - get_ct_image 成功后：连续二进制帧（长度+数据），长度 0 表示结束
 *
 * 线程：对象 moveToThread 到 ois-worker；socket 在该线程创建。
 */
#ifndef CBCT_OIS_SERVICE_H
#define CBCT_OIS_SERVICE_H

#include "ois/ois_types.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class QTcpSocket;

namespace cbct {

/**
 * @class OisService
 * @brief 下载/查询 OIS；上传接口预留（uploadSeries 目前返回未实现）
 */
class OisService : public QObject
{
    Q_OBJECT

public:
    explicit OisService(QObject *parent = nullptr);

    /// 服务器地址（默认 127.0.0.1:8080；环境变量 OIS_HOST / OIS_PORT 可覆盖）
    void setServer(const QString &host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

public slots:
    /// 连接服务器（异步，看 connected/errorOccurred）
    void connectToServer();

    /// 查询检查列表
    void queryStudies();

    /// 下载指定序列全部切片到本地数据目录
    void downloadSeries(const QString &patientId,
                        const QString &studyUid,
                        const QString &seriesUid);

    /// 上传序列（接口预留；模拟采集暂不实现真实上传）
    void uploadSeries(const QString &patientId,
                      const QString &studyUid,
                      const QString &seriesUid);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);

    void studiesReceived(const QList<cbct::OisStudy> &studies);
    void seriesReceived(const QList<cbct::OisSeries> &seriesList);

    void downloadProgress(int receivedFrames, int expectedFrames, qint64 bytes);
    void downloadFinished(const cbct::OisDownloadResult &result);

    /// 上传接口占位：当前总是发此错误
    void uploadNotImplemented(const QString &message);

private slots:
    void onReadyRead();
    void onSocketError();
    void onSocketConnected();
    void onSocketDisconnected();

private:
    enum class Phase {
        Idle,
        WaitJson,       ///< 等待 JSON 响应
        WaitBinary,     ///< 等待二进制切片帧
    };

    bool sendRequest(const QString &type, const QJsonObject &params);
    void resetRequestState();
    void handleJsonResponse(const QByteArray &jsonBytes);
    void appendBinaryFrame(const QByteArray &payload);
    void finishDownload(bool success, const QString &errorMessage);

    QTcpSocket *m_socket = nullptr; ///< 属 ois-worker 线程
    QString m_host;
    quint16 m_port = 8080;

    Phase m_phase = Phase::Idle;
    int m_nextRequestId = 1;
    int m_expectedFrames = 0;
    qint64 m_expectedBytes = 0;
    int m_receivedFrames = 0;
    qint64 m_receivedBytes = 0;

    /// 当前下载目标
    QString m_dlPatientId;
    QString m_dlStudyUid;
    QString m_dlSeriesUid;
    QString m_dlLocalDir;

    QByteArray m_readBuffer;     ///< 粘包缓冲
    QByteArray m_frameBody;      ///< 当前帧内容
    quint32 m_frameNeed = 0;     ///< 当前帧还需字节（0=尚未读长度）
    bool m_haveLength = false;

    /// 连接成功后自动执行的请求（避免 connect + 立刻 send 竞态）
    QString m_pendingRequestType;
    QJsonObject m_pendingRequestParams;
    bool m_hasPendingRequest = false;

    QList<OisStudy> m_pendingStudies;
    QList<OisSeries> m_pendingSeries;
};

} // namespace cbct

#endif // CBCT_OIS_SERVICE_H
