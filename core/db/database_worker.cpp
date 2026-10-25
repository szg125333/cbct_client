#include "db/database_worker.h"

#include "common/log_service.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cbct {

namespace {
const char kCreateDetectionTable[] =
    "CREATE TABLE IF NOT EXISTS detection_record ("
    "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  recipe_id   TEXT NOT NULL,"
    "  recipe_name TEXT NOT NULL,"
    "  status      TEXT NOT NULL,"
    "  note        TEXT NOT NULL DEFAULT '',"
    "  frame_count INTEGER NOT NULL DEFAULT 0,"
    "  finished_at TEXT NOT NULL"
    ")";

const char kCreateFinishedIndex[] =
    "CREATE INDEX IF NOT EXISTS idx_detection_finished_at "
    "ON detection_record(finished_at DESC)";
} // namespace

DatabaseWorker::DatabaseWorker(QObject *parent)
    : QObject(parent)
{
}

DatabaseWorker::~DatabaseWorker()
{
    if (m_open && !m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseWorker::open(const QString &dbPath)
{
    m_dbPath = dbPath;
    m_connectionName = QStringLiteral("cbct_db_")
        + QString::number(reinterpret_cast<quintptr>(this), 16);

    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase old = QSqlDatabase::database(m_connectionName, false);
            if (old.isValid()) {
                old.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        const QString err = QStringLiteral("打开数据库失败: %1 — %2")
                                .arg(dbPath, db.lastError().text());
        CBCT_LOG_ERROR("db", err.toStdString());
        m_open = false;
        emit errorOccurred(err);
        emit opened(false, err);
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

    // 必须先登记线程→连接，再 ensureTable（内部走 connectionForCurrentThread）
    m_connectionByThread.insert(QThread::currentThreadId(), m_connectionName);

    if (!ensureTable()) {
        m_connectionByThread.remove(QThread::currentThreadId());
        m_open = false;
        emit opened(false, QStringLiteral("建表失败"));
        return false;
    }

    m_open = true;
    CBCT_LOG_INFO("db", "数据库已打开: {}", dbPath.toStdString());
    emit opened(true, QStringLiteral("ok"));
    return true;
}

void DatabaseWorker::close()
{
    if (m_connectionName.isEmpty()) {
        return;
    }
    CBCT_LOG_DEBUG("db", "关闭数据库连接 {}", m_connectionName.toStdString());
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid()) {
            db.close();
        }
    }
    m_connectionByThread.remove(QThread::currentThreadId());
    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionName.clear();
    m_open = false;
}

bool DatabaseWorker::ensureTable()
{
    const QString conn = connectionForCurrentThread();
    if (conn.isEmpty()) {
        emit errorOccurred(QStringLiteral("当前线程无数据库连接"));
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(conn);
    QSqlQuery q(db);
    if (!q.exec(QLatin1String(kCreateDetectionTable))) {
        emit errorOccurred(QStringLiteral("CREATE TABLE 失败: %1").arg(q.lastError().text()));
        return false;
    }
    if (!q.exec(QLatin1String(kCreateFinishedIndex))) {
        emit errorOccurred(QStringLiteral("CREATE INDEX 失败: %1").arg(q.lastError().text()));
        return false;
    }
    return true;
}

QString DatabaseWorker::connectionForCurrentThread() const
{
    // 关键：每次 SQL 前都按「我是哪个线程」查 map，而不是盲目用成员变量
    const Qt::HANDLE tid = QThread::currentThreadId();
    const auto it = m_connectionByThread.constFind(tid);
    if (it == m_connectionByThread.constEnd()) {
        return QString(); // 陌生线程：不允许直接摸 QSqlDatabase
    }
    return it.value();
}

void DatabaseWorker::insertRecord(const DetectionRecord &record)
{
    if (!m_open) {
        emit errorOccurred(QStringLiteral("数据库未打开"));
        return;
    }

    // 必须先按线程 id 查 map，再拿连接（防止在错误线程用连接）
    const QString conn = connectionForCurrentThread();
    if (conn.isEmpty()) {
        emit errorOccurred(QStringLiteral("当前线程未注册数据库连接"));
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(conn);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO detection_record "
        "(recipe_id, recipe_name, status, note, frame_count, finished_at) "
        "VALUES (?, ?, ?, ?, ?, ?)"));

    const QDateTime finished =
        record.finishedAt.isValid() ? record.finishedAt : QDateTime::currentDateTime();
    q.addBindValue(record.recipeId);
    q.addBindValue(record.recipeName);
    q.addBindValue(record.status);
    q.addBindValue(record.note);
    q.addBindValue(record.frameCount);
    q.addBindValue(finished.toString(Qt::ISODate));

    if (!q.exec()) {
        const QString err = QStringLiteral("INSERT 失败: %1").arg(q.lastError().text());
        CBCT_LOG_ERROR("db", err.toStdString());
        emit errorOccurred(err);
        return;
    }
    const qint64 newId = q.lastInsertId().toLongLong();
    CBCT_LOG_INFO("db", "INSERT detection_record id={} recipe={} status={}",
                  newId, record.recipeName.toStdString(), record.status.toStdString());
    emit recordInserted(newId);
}

void DatabaseWorker::queryRecent(int limit)
{
    if (!m_open) {
        emit errorOccurred(QStringLiteral("数据库未打开"));
        emit recentRecords({});
        return;
    }

    const int lim = limit > 0 ? limit : 100;
    const QString conn = connectionForCurrentThread();
    if (conn.isEmpty()) {
        emit errorOccurred(QStringLiteral("当前线程未注册数据库连接"));
        emit recentRecords({});
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(conn);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, recipe_id, recipe_name, status, note, frame_count, finished_at "
        "FROM detection_record "
        "ORDER BY finished_at DESC, id DESC "
        "LIMIT ?"));
    q.addBindValue(lim);

    QList<DetectionRecord> out;
    if (!q.exec()) {
        emit errorOccurred(QStringLiteral("SELECT 失败: %1").arg(q.lastError().text()));
        emit recentRecords(out);
        return;
    }

    while (q.next()) {
        DetectionRecord r;
        r.id = q.value(0).toLongLong();
        r.recipeId = q.value(1).toString();
        r.recipeName = q.value(2).toString();
        r.status = q.value(3).toString();
        r.note = q.value(4).toString();
        r.frameCount = q.value(5).toInt();
        r.finishedAt = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
        out.append(r);
    }
    CBCT_LOG_DEBUG("db", "queryRecent limit={} count={}", lim, static_cast<int>(out.size()));
    emit recentRecords(out);
}

} // namespace cbct
