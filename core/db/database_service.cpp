#include "db/database_service.h"

#include "common/app_paths.h"
#include "common/log_service.h"
#include "db/database_worker.h"

#include <QMetaObject>

namespace cbct {

DatabaseService::DatabaseService(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread(this))
{
    // 跨线程 Q_ARG / 信号需要注册
    qRegisterMetaType<cbct::DetectionRecord>("cbct::DetectionRecord");
    qRegisterMetaType<QList<cbct::DetectionRecord>>("QList<cbct::DetectionRecord>");

    m_worker = new DatabaseWorker();
    m_worker->moveToThread(m_thread);
    m_thread->setObjectName(QStringLiteral("db-worker"));
    m_thread->start();

    connect(m_worker, &DatabaseWorker::opened, this,
            [this](bool ok, const QString &message) {
                if (ok) {
                    m_ready = true;
                    emit ready();
                } else {
                    emit dbError(message);
                }
            });
    connect(m_worker, &DatabaseWorker::recordInserted,
            this, &DatabaseService::recordInserted);
    connect(m_worker, &DatabaseWorker::recentRecords,
            this, &DatabaseService::recentRecordsReady);
    connect(m_worker, &DatabaseWorker::errorOccurred,
            this, &DatabaseService::dbError);
}

DatabaseService::~DatabaseService()
{
    shutdown();
}

void DatabaseService::shutdown()
{
    if (m_shutdownDone) {
        return;
    }
    m_shutdownDone = true;
    CBCT_LOG_DEBUG("db", "DatabaseService::shutdown");

    // 不使用 BlockingQueued：db 线程若卡住会导致关窗无法退出
    // 直接停事件循环；SQLite 连接由进程退出回收
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(1500);
    }
    m_worker = nullptr;
    m_ready = false;
}

bool DatabaseService::initialize(QString *error)
{
    if (m_initStarted) {
        return m_ready;
    }
    m_initStarted = true;
    CBCT_LOG_DEBUG("db", "DatabaseService::initialize");

    const QString path = paths::databaseFilePath();
    if (!paths::ensureDirFor(path)) {
        const QString msg = QStringLiteral("无法创建数据库目录: %1").arg(paths::appDataDir());
        if (error) {
            *error = msg;
        }
        emit dbError(msg);
        return false;
    }

    // BlockingQueued + Q_RETURN_ARG：同步拿到 open 的 bool
    // （不能只靠 opened 信号：Queued 回主线程时本函数还没跑事件循环）
    bool ok = false;
    const bool invoked = QMetaObject::invokeMethod(
        m_worker, "open", Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, ok),
        Q_ARG(QString, path));
    if (!invoked) {
        const QString msg = QStringLiteral("无法调用 db 线程 open");
        if (error) {
            *error = msg;
        }
        emit dbError(msg);
        return false;
    }
    if (!ok) {
        m_ready = false;
        const QString msg = QStringLiteral("数据库初始化失败: %1").arg(path);
        if (error) {
            *error = msg;
        }
        return false;
    }
    m_ready = true;
    CBCT_LOG_INFO("db", "DatabaseService 初始化成功 path={}", path.toStdString());
    return true;
}

void DatabaseService::insertRecord(const DetectionRecord &record)
{
    CBCT_LOG_DEBUG("db", "提交 INSERT 任务 recipe={}", record.recipeName.toStdString());
    QMetaObject::invokeMethod(m_worker, "insertRecord", Qt::QueuedConnection,
                              Q_ARG(cbct::DetectionRecord, record));
}

void DatabaseService::queryRecent(int limit)
{
    CBCT_LOG_DEBUG("db", "提交 queryRecent 任务 limit={}", limit);
    QMetaObject::invokeMethod(m_worker, "queryRecent", Qt::QueuedConnection,
                              Q_ARG(int, limit));
}

} // namespace cbct
