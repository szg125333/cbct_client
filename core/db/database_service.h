/**
 * @file database_service.h
 * @brief 数据库门面：任意线程投递任务，SQL 在 db 工作线程执行
 *
 * 工业模式：Task / Actor —— 调用方只 submit，不直接操作 QSqlDatabase。
 * SQLite 单写者：本服务内部只有一条连接（挂在 DatabaseWorker 上）。
 */
#ifndef CBCT_DATABASE_SERVICE_H
#define CBCT_DATABASE_SERVICE_H

#include "db/detection_record.h"

#include <QObject>
#include <QString>
#include <QThread>

namespace cbct {

class DatabaseWorker;

/**
 * @class DatabaseService
 * @brief ClientCore::database() 返回本类型；UI 经 RecordBridge 使用
 *
 * initialize() 后可 insert/query；结果一律用信号回到调用线程（Queued）。
 */
class DatabaseService : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseService(QObject *parent = nullptr);
    ~DatabaseService() override;

    /// 启动 db 线程并打开库+建表；幂等。失败通过 dbError。
    bool initialize(QString *error = nullptr);

    /// 退出前关闭连接并停线程（勿在析构里做长时间 Blocking）
    void shutdown();

    /// 任意线程：异步插入（结果看 recordInserted / dbError）
    void insertRecord(const DetectionRecord &record);

    /// 任意线程：异步查最近 limit 条（结果 recentRecordsReady）
    void queryRecent(int limit = 100);

    bool isReady() const { return m_ready; }

signals:
    /// 打开并建表成功
    void ready();
    /// 打开/建表失败
    void dbError(const QString &message);
    /// INSERT 完成（id 为自增主键）
    void recordInserted(qint64 id);
    /// queryRecent 完成
    void recentRecordsReady(const QList<cbct::DetectionRecord> &records);

private:
    DatabaseWorker *m_worker = nullptr; ///< 无 parent，moveToThread(db 线程)
    QThread *m_thread = nullptr;        ///< db 工作线程
    bool m_ready = false;               ///< open 是否已成功
    bool m_initStarted = false;         ///< 防止重复 initialize
    bool m_shutdownDone = false;        ///< shutdown 幂等
};

} // namespace cbct

#endif // CBCT_DATABASE_SERVICE_H
