/**
 * @file record_bridge.h
 * @brief app ↔ core 检测记录适配层：UI 不直接碰 QSqlDatabase / DatabaseWorker
 */
#ifndef RECORD_BRIDGE_H
#define RECORD_BRIDGE_H

#include "db/detection_record.h"

#include <QObject>

/**
 * @class RecordBridge
 * @brief 查询历史记录、提交一条采集结果；结果以信号回 UI 线程
 */
class RecordBridge : public QObject
{
    Q_OBJECT

public:
    explicit RecordBridge(QObject *parent = nullptr);

    /// 异步查最近 limit 条 → recordsUpdated
    void reloadRecent(int limit = 100);

    /// 异步写入一条（采集成功/失败都走这里）
    void appendRecord(const cbct::DetectionRecord &record);

signals:
    /// 收到最近记录列表（可能为空）
    void recordsUpdated(const QList<cbct::DetectionRecord> &list);
    /// 数据库相关失败（初始化/查询/写入），UI 需明确提示
    void databaseError(const QString &message);
};

#endif // RECORD_BRIDGE_H
