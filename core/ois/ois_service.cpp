#include "ois/ois_service.h"

#include "common/app_paths.h"
#include "common/log_service.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMetaType>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

#include <algorithm>

namespace cbct {

namespace {
/// 默认服务器：云端 ois_server（ois.conf port=8080）；OIS_HOST/OIS_PORT 可覆盖
const char kDefaultHost[] = "47.100.193.91";
const quint16 kDefaultPort = 8080;

} // namespace

OisService::OisService(QObject *parent)
    : QObject(parent)
{
    // 跨线程信号携带的自定义类型必须注册，否则 QueuedConnection 丢信号
    qRegisterMetaType<cbct::OisStudy>("cbct::OisStudy");
    qRegisterMetaType<QList<cbct::OisStudy>>("QList<cbct::OisStudy>");
    qRegisterMetaType<cbct::OisSeries>("cbct::OisSeries");
    qRegisterMetaType<QList<cbct::OisSeries>>("QList<cbct::OisSeries>");
    qRegisterMetaType<cbct::OisDownloadResult>("cbct::OisDownloadResult");

    const QByteArray hostEnv = qgetenv("OIS_HOST");
    const QByteArray portEnv = qgetenv("OIS_PORT");
    m_host = hostEnv.isEmpty() ? QLatin1String(kDefaultHost) : QString::fromLocal8Bit(hostEnv);
    m_port = portEnv.isEmpty() ? kDefaultPort : static_cast<quint16>(portEnv.toUInt());
}

void OisService::setServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
}

void OisService::connectToServer()
{
    if (!m_socket) {
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::connected, this, &OisService::onSocketConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &OisService::onSocketDisconnected);
        connect(m_socket, &QTcpSocket::readyRead, this, &OisService::onReadyRead);
        // Qt 5.14+ 推荐 errorOccurred
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        connect(m_socket, &QAbstractSocket::errorOccurred, this, &OisService::onSocketError);
#else
        connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                this, &OisService::onSocketError);
#endif
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        emit connected();
        return;
    }

    CBCT_LOG_INFO("ois", "连接服务器 {}:{}", m_host.toStdString(), m_port);
    // 丢弃半开/残留连接与缓冲，避免旧状态污染新会话
    resetRequestState();
    m_readBuffer.clear();
    m_socket->abort();
    m_socket->connectToHost(m_host, m_port);
}

void OisService::queryStudies()
{
    // 新请求：丢掉上一次残留的半包/状态，避免脏缓冲吞掉响应
    resetRequestState();
    m_readBuffer.clear();

    m_pendingRequestType = QStringLiteral("query_study");
    m_pendingRequestParams = QJsonObject();
    m_hasPendingRequest = true;

    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        connectToServer();
        return;
    }
    sendRequest(m_pendingRequestType, m_pendingRequestParams);
    m_hasPendingRequest = false;
    m_phase = Phase::WaitJson;
}

void OisService::downloadSeries(const QString &patientId,
                                const QString &studyUid,
                                const QString &seriesUid)
{
    CBCT_LOG_INFO("ois", "请求下载 patient={} study={} series={}",
                  patientId.toStdString(), studyUid.toStdString(), seriesUid.toStdString());

    resetRequestState();
    m_readBuffer.clear();

    m_dlPatientId = patientId;
    m_dlStudyUid = studyUid;
    m_dlSeriesUid = seriesUid;
    m_expectedFrames = 0;
    m_expectedBytes = 0;
    m_receivedFrames = 0;
    m_receivedBytes = 0;

    // 本地目录：ois_downloads/{patient}/{study}/{series}/
    m_dlLocalDir = paths::oisDownloadRootDir() + QLatin1Char('/') + patientId
        + QLatin1Char('/') + studyUid + QLatin1Char('/') + seriesUid;
    if (!paths::ensureDirFor(m_dlLocalDir + QStringLiteral("/x"))) {
        emit errorOccurred(QStringLiteral("无法创建下载目录: %1").arg(m_dlLocalDir));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("patient_id")] = m_dlPatientId;
    params[QStringLiteral("study_uid")] = m_dlStudyUid;
    params[QStringLiteral("series_uid")] = m_dlSeriesUid;
    m_pendingRequestType = QStringLiteral("get_ct_image");
    m_pendingRequestParams = params;
    m_hasPendingRequest = true;

    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        connectToServer();
        return;
    }
    sendRequest(m_pendingRequestType, m_pendingRequestParams);
    m_hasPendingRequest = false;
    m_phase = Phase::WaitJson;
}

void OisService::uploadSeries(const QString &patientId,
                              const QString &studyUid,
                              const QString &seriesUid)
{
    Q_UNUSED(patientId)
    Q_UNUSED(studyUid)
    Q_UNUSED(seriesUid)
    // 预留：采集完成后上传；当前模拟采集不落真实 DICOM
    const QString msg = QStringLiteral("上传接口已预留，模拟采集阶段暂未实现");
    CBCT_LOG_WARN("ois", msg.toStdString());
    emit uploadNotImplemented(msg);
}

bool OisService::sendRequest(const QString &type, const QJsonObject &params)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("socket 未连接"));
        return false;
    }

    QJsonObject req;
    req[QStringLiteral("type")] = type;
    req[QStringLiteral("id")] = m_nextRequestId++;
    req[QStringLiteral("params")] = params;

    // 服务端 PROTO_CUSTOM：按行读 JSON（\n 结尾），不带长度前缀
    QByteArray frame = QJsonDocument(req).toJson(QJsonDocument::Compact);
    frame.append('\n');

    const qint64 n = m_socket->write(frame);
    if (n != frame.size()) {
        emit errorOccurred(QStringLiteral("发送请求失败"));
        return false;
    }
    CBCT_LOG_DEBUG("ois", "发送请求 type={} len={}", type.toStdString(), frame.size());
    return true;
}

void OisService::onSocketConnected()
{
    CBCT_LOG_INFO("ois", "已连接 {}:{}", m_host.toStdString(), m_port);
    emit connected();

    // 连接成功后补发挂起的请求
    if (m_hasPendingRequest && !m_pendingRequestType.isEmpty()) {
        sendRequest(m_pendingRequestType, m_pendingRequestParams);
        m_hasPendingRequest = false;
        m_phase = Phase::WaitJson;
    }
}

void OisService::onSocketDisconnected()
{
    CBCT_LOG_INFO("ois", "连接断开 phase={}", static_cast<int>(m_phase));
    const bool hadRequest = m_phase != Phase::Idle || m_hasPendingRequest;
    resetRequestState();
    m_readBuffer.clear();
    m_hasPendingRequest = false;
    emit disconnected();
    // 下载/查询中途被断：必须通知 UI 复位按钮，否则会一直“没反应”
    if (hadRequest) {
        emit errorOccurred(QStringLiteral("连接已断开"));
    }
}

void OisService::onSocketError()
{
    const QString msg = m_socket ? m_socket->errorString() : QStringLiteral("socket error");
    CBCT_LOG_ERROR("ois", msg.toStdString());
    const bool hadRequest = m_phase != Phase::Idle || m_hasPendingRequest;
    resetRequestState();
    m_readBuffer.clear();
    m_hasPendingRequest = false;
    emit errorOccurred(msg);
    Q_UNUSED(hadRequest)
}

void OisService::resetRequestState()
{
    m_phase = Phase::Idle;
    m_haveLength = false;
    m_frameNeed = 0;
    m_frameBody.clear();
    m_expectedFrames = 0;
    m_expectedBytes = 0;
    m_receivedFrames = 0;
    m_receivedBytes = 0;
}

void OisService::onReadyRead()
{
    if (!m_socket) {
        return;
    }
    m_readBuffer.append(m_socket->readAll());

    // 循环解析：先 4 字节长度，再 body
    while (true) {
        if (!m_haveLength) {
            if (m_readBuffer.size() < 4) {
                return;
            }
            const quint32 netLen = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(m_readBuffer.constData()));
            m_readBuffer.remove(0, 4);
            m_frameNeed = netLen;
            m_haveLength = true;
            m_frameBody.clear();
            m_frameBody.reserve(static_cast<int>(netLen));
        }

        const qint64 remain = static_cast<qint64>(m_frameNeed) - m_frameBody.size();
        if (remain > 0) {
            if (m_readBuffer.size() < remain) {
                m_frameBody.append(m_readBuffer);
                m_readBuffer.clear();
                return;
            }
            m_frameBody.append(m_readBuffer.constData(), static_cast<int>(remain));
            m_readBuffer.remove(0, static_cast<int>(remain));
        }

        // 一帧收齐
        m_haveLength = false;
        const quint32 frameLen = m_frameNeed;
        m_frameNeed = 0;

        if (m_phase == Phase::WaitJson) {
            handleJsonResponse(m_frameBody);
        } else if (m_phase == Phase::WaitBinary) {
            if (frameLen == 0) {
                // 结束标记
                finishDownload(true, QString());
                return;
            }
            appendBinaryFrame(m_frameBody);
        }
        m_frameBody.clear();
    }
}

void OisService::handleJsonResponse(const QByteArray &jsonBytes)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        CBCT_LOG_ERROR("ois", "JSON 响应解析失败: {}", err.errorString().toStdString());
        emit errorOccurred(QStringLiteral("JSON 响应解析失败: %1").arg(err.errorString()));
        resetRequestState();
        return;
    }

    const QJsonObject obj = doc.object();
    const QString status = obj.value(QStringLiteral("status")).toString();
    const QJsonObject data = obj.value(QStringLiteral("data")).toObject();

    if (status != QLatin1String("ok")) {
        const QString msg = data.value(QStringLiteral("error")).toString(
            QStringLiteral("服务器返回错误"));
        CBCT_LOG_ERROR("ois", "业务错误: {}", msg.toStdString());
        emit errorOccurred(msg);
        resetRequestState();
        return;
    }

    // 判断是否 query_study：data.studies
    if (data.contains(QStringLiteral("studies"))) {
        QList<OisStudy> studies;
        const QJsonArray arr = data.value(QStringLiteral("studies")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject s = v.toObject();
            OisStudy item;
            item.studyId = s.value(QStringLiteral("study_id")).toString();
            item.patientName = s.value(QStringLiteral("patient_name")).toString();
            item.modality = s.value(QStringLiteral("modality")).toString();
            item.studyDate = s.value(QStringLiteral("study_date")).toString();
            item.studyDesc = s.value(QStringLiteral("study_desc")).toString();
            studies.append(item);
        }
        CBCT_LOG_INFO("ois", "收到检查列表 count={}", static_cast<int>(studies.size()));
        emit studiesReceived(studies);
        resetRequestState();
        return;
    }

    // query_series
    if (data.contains(QStringLiteral("series"))) {
        QList<OisSeries> list;
        const QJsonArray arr = data.value(QStringLiteral("series")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject s = v.toObject();
            OisSeries item;
            item.seriesId = s.value(QStringLiteral("series_id")).toString();
            item.seriesNum = s.value(QStringLiteral("series_num")).toInt();
            item.seriesDesc = s.value(QStringLiteral("series_desc")).toString();
            item.instanceCount = s.value(QStringLiteral("instance_count")).toInt();
            list.append(item);
        }
        CBCT_LOG_INFO("ois", "收到序列列表 count={}", static_cast<int>(list.size()));
        emit seriesReceived(list);
        resetRequestState();
        return;
    }

    // get_ct_image：JSON 后还有二进制帧
    if (data.contains(QStringLiteral("binary_follows"))
        || data.contains(QStringLiteral("slice_count"))) {
        m_expectedFrames = data.value(QStringLiteral("slice_count")).toInt();
        m_expectedBytes = static_cast<qint64>(
            data.value(QStringLiteral("total_size")).toDouble());
        m_phase = Phase::WaitBinary;
        CBCT_LOG_INFO("ois", "开始接收二进制切片 expectedFrames={} totalBytes={}",
                      m_expectedFrames, m_expectedBytes);
        emit downloadProgress(0, m_expectedFrames, 0);
        return;
    }

    // 其它：无后续二进制，直接结束（避免卡在 WaitJson）
    CBCT_LOG_WARN("ois", "未识别的 data 结构，结束当前请求");
    resetRequestState();
}

void OisService::appendBinaryFrame(const QByteArray &payload)
{
    const int index = m_receivedFrames;
    const QString fileName = QStringLiteral("slice_%1.dcm")
                                 .arg(index, 4, 10, QLatin1Char('0'));
    const QString fullPath = m_dlLocalDir + QLatin1Char('/') + fileName;

    QFile f(fullPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        CBCT_LOG_ERROR("ois", "写文件失败: {}", fullPath.toStdString());
        finishDownload(false, QStringLiteral("写文件失败: %1").arg(fullPath));
        return;
    }
    f.write(payload);
    f.close();

    m_receivedFrames++;
    m_receivedBytes += payload.size();
    emit downloadProgress(m_receivedFrames, m_expectedFrames, m_receivedBytes);
}

void OisService::finishDownload(bool success, const QString &errorMessage)
{
    if (success) {
        OisDownloadResult r;
        r.localDir = m_dlLocalDir;
        r.fileCount = m_receivedFrames;
        r.totalBytes = m_receivedBytes;
        r.patientId = m_dlPatientId;
        r.studyUid = m_dlStudyUid;
        r.seriesUid = m_dlSeriesUid;
        CBCT_LOG_INFO("ois", "下载完成 files={} bytes={} dir={}",
                      r.fileCount, static_cast<qint64>(r.totalBytes),
                      r.localDir.toStdString());
        resetRequestState();
        emit downloadFinished(r);
    } else {
        CBCT_LOG_ERROR("ois", "下载失败: {}", errorMessage.toStdString());
        resetRequestState();
        emit errorOccurred(errorMessage);
    }
}

} // namespace cbct
