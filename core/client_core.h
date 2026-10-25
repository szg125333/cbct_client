/**
 * @file client_core.h
 * @brief 核心门面（Facade）：进程内唯一入口，app 只通过 instance() 访问各子模块
 *
 * 设计意图：
 * - 高内聚：配方、平板采集、SQLite 等逻辑都收在 libcbct_core.so
 * - 低耦合：UI 不直接 new 业务对象，只拿 ClientCore 上的 accessor
 * - 可扩展：以后加设备通信等，只加成员 + accessor
 */
#ifndef CBCT_CLIENT_CORE_H
#define CBCT_CLIENT_CORE_H

#include <QObject>
#include <QThread>

namespace cbct {

class RecipeManager;      ///< 配方管理（JSON 持久化）
class DetectorSimulator;  ///< 平板采集模拟器（工作线程）
class FrameMailbox;       ///< 采集帧有界队列
class DatabaseService;    ///< SQLite 服务（db 工作线程）

/**
 * @class ClientCore
 * @brief 单例门面；UI 启动时调用一次 initialize()
 *
 * 线程模型：
 * - recipeManager()：主线程
 * - detector()：detector-worker 线程
 * - frameMailbox()：采集写 / UI 读
 * - database()：db-worker 线程；insert/query 从任意线程投递任务
 */
class ClientCore : public QObject
{
    Q_OBJECT

public:
    static ClientCore &instance();

    ClientCore(const ClientCore &) = delete;
    ClientCore &operator=(const ClientCore &) = delete;

    void initialize();

    /// 退出前必须调用：停采集线程、关数据库线程（aboutToQuit 时调用）
    void shutdown();

    RecipeManager &recipeManager();
    DetectorSimulator &detector();
    FrameMailbox &frameMailbox();
    DatabaseService &database();

signals:
    void ready();

private:
    explicit ClientCore(QObject *parent = nullptr);
    ~ClientCore() override;

    RecipeManager *m_recipeManager = nullptr;
    DetectorSimulator *m_detector = nullptr;
    FrameMailbox *m_mailbox = nullptr;
    DatabaseService *m_database = nullptr; ///< SQLite 门面
    QThread *m_detectorThread = nullptr;
    bool m_initialized = false;
    bool m_shutdownDone = false; ///< shutdown 幂等
};

} // namespace cbct

#endif // CBCT_CLIENT_CORE_H
