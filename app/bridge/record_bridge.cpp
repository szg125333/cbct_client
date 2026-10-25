#include "bridge/record_bridge.h"

#include "client_core.h"
#include "db/database_service.h"

RecordBridge::RecordBridge(QObject *parent)
    : QObject(parent)
{
    // DatabaseService 在 ClientCore（通常主线程）；信号到本对象为 Direct/同线程
    auto &db = cbct::ClientCore::instance().database();
    connect(&db, &cbct::DatabaseService::recentRecordsReady,
            this, &RecordBridge::recordsUpdated);
    connect(&db, &cbct::DatabaseService::dbError,
            this, &RecordBridge::databaseError);
}

void RecordBridge::reloadRecent(int limit)
{
    cbct::ClientCore::instance().initialize();
    cbct::ClientCore::instance().database().queryRecent(limit);
}

void RecordBridge::appendRecord(const cbct::DetectionRecord &record)
{
    cbct::ClientCore::instance().initialize();
    cbct::ClientCore::instance().database().insertRecord(record);
}
