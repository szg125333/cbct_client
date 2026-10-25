/**
 * @file detector_bridge.h
 * @brief app ↔ core 平板适配层：UI 只依赖本类与 ClientCore，不直接摸工作线程细节
 */
#ifndef DETECTOR_BRIDGE_H
#define DETECTOR_BRIDGE_H

#include "device/frame_mailbox.h"
#include "recipe/recipe_params.h"

#include <QObject>

/**
 * @class DetectorBridge
 * @brief 启动/停止采集（跨线程调 core）；UI 从队列拉帧显示
 *
 * 使用约定：本对象属于 UI 线程。
 */
class DetectorBridge : public QObject
{
    Q_OBJECT

public:
    explicit DetectorBridge(QObject *parent = nullptr);

    /**
     * @brief 配置 + 初始化 + 启动（同步等待工作线程 beginAcquisition 返回）
     * @param recipe 当前选中配方（用其中 detector 参数）
     * @param error 失败原因
     */
    bool startWithRecipe(const cbct::Recipe &recipe, QString *error = nullptr);

    /// 异步停止（Queued 到工作线程 endAcquisition）
    void stop();

    /// UI 线程：非阻塞取一帧
    bool tryPopFrame(cbct::ProjectionFrame &out);

    /// 队列中待显示帧数（调试）
    int pendingFrames() const;

signals:
    void stateText(const QString &text);       ///< 「运行中/空闲/…」给状态栏
    void acquisitionFinished(int totalFrames); ///< 全部帧推完
    void acquisitionError(const QString &message);
    void started();
    void stopped();
};

#endif // DETECTOR_BRIDGE_H
