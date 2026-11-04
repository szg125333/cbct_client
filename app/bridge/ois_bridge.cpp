#include "bridge/ois_bridge.h"

#include "client_core.h"
#include "common/log_service.h"
#include "ois/ois_service.h"

#include <QMetaObject>

OisBridge::OisBridge(QObject *parent)
    : QObject(parent)
{
    auto &svc = cbct::ClientCore::instance().ois();

    connect(&svc, &cbct::OisService::connected, this, [this]() {
        emit statusText(QStringLiteral("OIS 已连接"));
    });
    connect(&svc, &cbct::OisService::disconnected, this, [this]() {
        emit statusText(QStringLiteral("OIS 已断开"));
    });
    connect(&svc, &cbct::OisService::errorOccurred, this, &OisBridge::failed);
    connect(&svc, &cbct::OisService::studiesReceived, this, &OisBridge::studiesReady);
    connect(&svc, &cbct::OisService::downloadProgress, this, &OisBridge::downloadProgress);
    connect(&svc, &cbct::OisService::downloadFinished, this, &OisBridge::downloadDone);
    connect(&svc, &cbct::OisService::uploadNotImplemented, this, &OisBridge::uploadNotReady);
}

void OisBridge::fetchStudies()
{
    cbct::ClientCore::instance().initialize();
    emit statusText(QStringLiteral("正在查询 OIS…"));
    QMetaObject::invokeMethod(&cbct::ClientCore::instance().ois(), "queryStudies",
                              Qt::QueuedConnection);
}

void OisBridge::download(const QString &patientId, const QString &studyUid, const QString &seriesUid)
{
    cbct::ClientCore::instance().initialize();
    emit statusText(QStringLiteral("开始下载…"));
    QMetaObject::invokeMethod(&cbct::ClientCore::instance().ois(), "downloadSeries",
                              Qt::QueuedConnection,
                              Q_ARG(QString, patientId),
                              Q_ARG(QString, studyUid),
                              Q_ARG(QString, seriesUid));
}

void OisBridge::upload(const QString &patientId, const QString &studyUid, const QString &seriesUid)
{
    cbct::ClientCore::instance().initialize();
    QMetaObject::invokeMethod(&cbct::ClientCore::instance().ois(), "uploadSeries",
                              Qt::QueuedConnection,
                              Q_ARG(QString, patientId),
                              Q_ARG(QString, studyUid),
                              Q_ARG(QString, seriesUid));
}
