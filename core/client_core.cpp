#include "client_core.h"

#include "common/log_service.h"
#include "db/database_service.h"
#include "device/detector_simulator.h"
#include "device/frame_mailbox.h"
#include "recipe/recipe_manager.h"

namespace cbct {

ClientCore &ClientCore::instance()
{
    static ClientCore core;
    return core;
}

void ClientCore::initialize()
{
    if (m_initialized) {
        return;
    }
    CBCT_LOG_INFO("core", "ClientCore::initialize 开始");
    m_recipeManager->load();
    const bool dbOk = m_database->initialize();
    CBCT_LOG_INFO("core", "ClientCore::initialize 完成 db_ok={}", dbOk ? 1 : 0);
    m_initialized = true;
    emit ready();
}

void ClientCore::shutdown()
{
    if (m_shutdownDone) {
        return;
    }
    m_shutdownDone = true;
    CBCT_LOG_INFO("core", "ClientCore::shutdown 开始");

    // 一律不用 BlockingQueued：避免目标线程已忙/已退出时关窗死等

    // 1) 唤醒可能阻塞在队列上的采集线程
    if (m_mailbox) {
        m_mailbox->abort();
    }

    // 2) 停 detector 线程事件循环（定时器随线程结束）
    if (m_detectorThread) {
        m_detectorThread->quit();
        // 有限等待；超时也继续，保证进程能退出
        m_detectorThread->wait(1500);
    }

    // 3) 停数据库线程
    if (m_database) {
        m_database->shutdown();
    }

    m_initialized = false;
    CBCT_LOG_INFO("core", "ClientCore::shutdown 完成");
}

RecipeManager &ClientCore::recipeManager()
{
    return *m_recipeManager;
}

DetectorSimulator &ClientCore::detector()
{
    return *m_detector;
}

FrameMailbox &ClientCore::frameMailbox()
{
    return *m_mailbox;
}

DatabaseService &ClientCore::database()
{
    return *m_database;
}

ClientCore::ClientCore(QObject *parent)
    : QObject(parent)
    , m_recipeManager(new RecipeManager(this))
    , m_mailbox(new FrameMailbox(16))
    , m_detectorThread(new QThread(this))
{
    m_detector = new DetectorSimulator();
    m_detector->setFrameMailbox(m_mailbox);
    m_detector->moveToThread(m_detectorThread);
    m_detectorThread->setObjectName(QStringLiteral("detector-worker"));
    m_detectorThread->start();

    m_database = new DatabaseService(this);
}

ClientCore::~ClientCore()
{
    shutdown();

    if (m_detectorThread) {
        m_detectorThread->quit();
        m_detectorThread->wait(500);
    }
    delete m_mailbox;
    m_mailbox = nullptr;
}

} // namespace cbct
