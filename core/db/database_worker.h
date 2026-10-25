/**
 * @file database_worker.h
 * @brief 数据库工作对象：只在 db 工作线程上执行 SQL（单连接）
 *
 * 线程模型（SQLite 工业界常用）：
 * - 所有 QSqlDatabase 操作都发生在 DatabaseWorker 所在线程
 * - 进程内通常只有一条写连接 → 避免多线程写同一 .db 文件 SQLITE_BUSY
 * - 其它线程绝不直接碰 QSqlDatabase，只通过 DatabaseService 投递任务
 *
 * 关于「线程 id → 连接」map：
 * - 本类保留 m_connectionByThread 作扩展点说明
 * - 当前仅在 open() 时登记 db 工作线程自己的一条连接
 * - 若将来多连接读写分离，需开启 WAL 并限制写连接数量
 */
#ifndef CBCT_DATABASE_WORKER_H
#define CBCT_DATABASE_WORKER_H

#include "db/detection_record.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QThread>

class QSqlDatabase;

namespace cbct {

/**
 * @class DatabaseWorker
 * @brief moveToThread 到 DatabaseService 创建的 db 线程；槽在此线程执行
 */
class DatabaseWorker : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseWorker(QObject *parent = nullptr);
    ~DatabaseWorker() override;

public slots:
    /**
     * @brief 打开 SQLite 文件并建表（在 db 线程调用）
     * @param dbPath 绝对路径，如 …/CBCT Client/client.db
     * @return 是否成功（可用 BlockingQueued 同步取回）
     */
    bool open(const QString &dbPath);

    /// 关闭连接并移除 QSqlDatabase（析构/退出前）
    void close();

    /// 插入一条检测记录；成功发 recordInserted，失败发 errorOccurred
    void insertRecord(const cbct::DetectionRecord &record);

    /// 查询最近 limit 条（按 finished_at 降序）；结果发 recentRecords
    void queryRecent(int limit);

signals:
    /// open/close 完成
    void opened(bool ok, const QString &message);
    /// INSERT 成功后回传自增 id
    void recordInserted(qint64 id);
    /// queryRecent 结果（可为空列表）
    void recentRecords(const QList<cbct::DetectionRecord> &records);
    /// 任意 SQL/打开失败
    void errorOccurred(const QString &message);

private:
    bool ensureTable();

    /**
     * @brief 按「当前线程 id」在 map 中查找可用连接名
     *
     * 工业 SQLite 单写者模型：
     * - map 里通常只有 db 工作线程一条
     * - 若当前线程就是登记过的那条 → 返回该连接
     * - 若未登记 → 返回空（禁止在陌生线程上直接用 QSqlDatabase）
     */
    QString connectionForCurrentThread() const;

    /// 线程 id → 连接名；open 时登记 db 线程，SQL 路径必须经 connectionForCurrentThread()
    /// QThread::currentThreadId() 返回 Qt::HANDLE（void*）
    QHash<Qt::HANDLE, QString> m_connectionByThread;
    QString m_dbPath;           ///< 已打开的库文件路径
    bool m_open = false;        ///< 是否已成功 open
    QString m_connectionName;   ///< QSqlDatabase 连接名（唯一，如 "cbct_db_0"）
};

} // namespace cbct

#endif // CBCT_DATABASE_WORKER_H
