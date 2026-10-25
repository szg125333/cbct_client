#include "client_core.h"

#include "device/detector_simulator.h"
#include "device/frame_mailbox.h"
#include "recipe/recipe_manager.h"

namespace cbct {

ClientCore &ClientCore::instance()
{
    // 函数内 static：首次调用时构造；C++11 起线程安全
    static ClientCore core;
    return core;
}

void ClientCore::initialize()
{
    if (m_initialized) {
        return; // 已初始化则直接返回（幂等）
    }
    // 从 recipes.json 加载；文件不存在时会写入内置示例配方
    m_recipeManager->load();
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

ClientCore::ClientCore(QObject *parent)
    : QObject(parent)
    , m_recipeManager(new RecipeManager(this)) // 主线程对象，parent=this
    , m_mailbox(new FrameMailbox(16))          // 显示队列深度：最多缓存 16 帧，满则采集线程阻塞
    , m_detectorThread(new QThread(this))
{
    // 关键：detector 不能带 parent，否则无法 moveToThread
    m_detector = new DetectorSimulator();
    m_detector->setFrameMailbox(m_mailbox);
    m_detector->moveToThread(m_detectorThread); // 之后该对象的槽都在工作线程执行
    m_detectorThread->setObjectName(QStringLiteral("detector-worker"));
    m_detectorThread->start();
}

ClientCore::~ClientCore()
{
    // 先停工作线程事件循环，再释放对象，避免跨线程悬空
    if (m_detectorThread) {
        m_detectorThread->quit();
        m_detectorThread->wait(3000); // 最多等 3s
    }
    delete m_detector;
    delete m_mailbox;
}

} // namespace cbct
