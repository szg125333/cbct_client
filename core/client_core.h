/**
 * @file client_core.h
 * @brief 核心门面（Facade）：进程内唯一入口，app 只通过 instance() 访问各子模块
 *
 * 设计意图：
 * - 高内聚：配方、平板采集等逻辑都收在 libcbct_core.so
 * - 低耦合：UI 不直接 new 业务对象，只拿 ClientCore 上的 accessor
 * - 可扩展：以后加设备通信、日志等，只加成员 + accessor，不改 app 依赖结构
 */
#ifndef CBCT_CLIENT_CORE_H
#define CBCT_CLIENT_CORE_H

#include <QObject>
#include <QThread>

namespace cbct {

class RecipeManager;     ///< 配方管理（JSON 持久化）
class DetectorSimulator; ///< 平板采集模拟器（运行在工作线程）
class FrameMailbox;      ///< 采集帧有界队列（生产者→消费者）

/**
 * @class ClientCore
 * @brief 单例门面；UI 启动时调用一次 initialize()
 *
 * 线程模型：
 * - recipeManager()：主线程使用（读写本地 JSON）
 * - detector()：对象在 m_detectorThread 工作线程，跨线程调用须用 Qt 队列/阻塞队列连接
 * - frameMailbox()：线程安全，采集线程 push、UI 线程 tryPop
 */
class ClientCore : public QObject
{
    Q_OBJECT

public:
    /// 进程内唯一实例（Meyers 单例，线程安全初始化）
    static ClientCore &instance();

    ClientCore(const ClientCore &) = delete;
    ClientCore &operator=(const ClientCore &) = delete;

    /// 幂等初始化：加载配方等；由 main() 在 QApplication 设置好组织名/应用名后调用
    void initialize();

    /// 配方管理器（主线程）
    RecipeManager &recipeManager();

    /// 平板模拟器（对象属于工作线程）
    DetectorSimulator &detector();

    /// 帧邮箱：采集线程写入、UI 读取
    FrameMailbox &frameMailbox();

signals:
    /// initialize() 完成后发一次
    void ready();

private:
    explicit ClientCore(QObject *parent = nullptr);
    ~ClientCore() override;

    RecipeManager *m_recipeManager = nullptr; ///< 配方 CRUD + recipes.json 读写
    DetectorSimulator *m_detector = nullptr;  ///< 采集模拟器；无 parent，已 moveToThread
    FrameMailbox *m_mailbox = nullptr;        ///< 有界帧队列，默认容量见构造函数
    QThread *m_detectorThread = nullptr;      ///< 工作线程；析构时 quit + wait
    bool m_initialized = false;               ///< 防止 initialize() 重复执行
};

} // namespace cbct

#endif // CBCT_CLIENT_CORE_H
