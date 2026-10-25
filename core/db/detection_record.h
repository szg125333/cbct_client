/**
 * @file detection_record.h
 * @brief 一条检测/采集记录的领域结构（库表 detection_record 对应）
 *
 * 仅数据，不含 SQL；由 DatabaseWorker 负责读写。
 */
#ifndef CBCT_DETECTION_RECORD_H
#define CBCT_DETECTION_RECORD_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QMetaType>

namespace cbct {

/**
 * @struct DetectionRecord
 * @brief 一次采集结束后的持久化记录
 *
 * 时间用 ISO 字符串存 SQLite（finished_at TEXT），读回时再解析为 QDateTime。
 */
struct DetectionRecord {
    qint64 id = 0;              ///< 自增主键；SELECT 带回；INSERT 后为 0 或 sqlite lastInsertId
    QString recipeId;           ///< 配方 UUID
    QString recipeName;         ///< 配方显示名（冗余列，列表展示无需 join）
    QString status;             ///< 结果状态，如「成功」「失败」
    QString note;               ///< 备注：帧数说明或错误信息
    QDateTime finishedAt;       ///< 采集结束时刻（本地时间）
    int frameCount = 0;         ///< 本次投影帧数；失败可为 0
};

} // namespace cbct

// 跨线程 Queued 信号需要注册元类型
Q_DECLARE_METATYPE(cbct::DetectionRecord)
Q_DECLARE_METATYPE(QList<cbct::DetectionRecord>)

#endif // CBCT_DETECTION_RECORD_H
