#include "client_core.h"

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
    m_recipeManager->load();
    // 打开 SQLite（client.db）并建表；失败只发 dbError，不中断 UI
    m_database->initialize();
    m_initialized = true;
    emit ready();
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

    // DatabaseService 自己管理 db 线程
    m_database = new DatabaseService(this);
}

ClientCore::~ClientCore()
{
    if (m_detectorThread) {
        m_detectorThread->quit();
        m_detectorThread->wait(3000);
    }
    delete m_detector;
    delete m_mailbox;
}

} // namespace cbct
