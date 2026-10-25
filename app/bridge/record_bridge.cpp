#include "bridge/record_bridge.h"

#include "client_core.h"
#include "common/log_service.h"
#include "db/database_service.h"

RecordBridge::RecordBridge(QObject *parent)
    : QObject(parent)
{
    auto &db = cbct::ClientCore::instance().database();
    connect(&db, &cbct::DatabaseService::recentRecordsReady,
            this, &RecordBridge::recordsUpdated);
    connect(&db, &cbct::DatabaseService::dbError,
            this, &RecordBridge::databaseError);
}

void RecordBridge::reloadRecent(int limit)
{
    CBCT_LOG_DEBUG("bridge", "请求加载最近检测记录 limit={}", limit);
    cbct::ClientCore::instance().initialize();
    cbct::ClientCore::instance().database().queryRecent(limit);
}

void RecordBridge::appendRecord(const cbct::DetectionRecord &record)
{
    CBCT_LOG_DEBUG("bridge", "请求写入检测记录 recipe={} status={}",
                   record.recipeName.toStdString(), record.status.toStdString());
    cbct::ClientCore::instance().initialize();
    cbct::ClientCore::instance().database().insertRecord(record);
}
