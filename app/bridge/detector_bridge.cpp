#include "bridge/detector_bridge.h"

#include "client_core.h"
#include "common/app_paths.h"
#include "device/detector_simulator.h"
#include "device/detector_types.h"

#include <QMetaObject>

DetectorBridge::DetectorBridge(QObject *parent)
    : QObject(parent)
{
    // detector 在工作线程；信号连到本对象（UI 线程）时自动变为 QueuedConnection
    auto &det = cbct::ClientCore::instance().detector();

    connect(&det, &cbct::DetectorSimulator::finished, this, [this]() {
        auto &d = cbct::ClientCore::instance().detector();
        emit acquisitionFinished(d.frameCount());
    });

    connect(&det, &cbct::DetectorSimulator::errorOccurred,
            this, &DetectorBridge::acquisitionError);

    // 状态枚举以 int 传输，避免跨线程注册复杂 enum
    connect(&det, &cbct::DetectorSimulator::stateChanged,
            this, [this](int s) {
                switch (static_cast<cbct::DetectorState>(s)) {
                case cbct::DetectorState::Idle:
                    emit stateText(QStringLiteral("空闲"));
                    break;
                case cbct::DetectorState::ApplyingParams:
                    emit stateText(QStringLiteral("下发参数"));
                    break;
                case cbct::DetectorState::Initializing:
                    emit stateText(QStringLiteral("初始化"));
                    break;
                case cbct::DetectorState::Running:
                    emit stateText(QStringLiteral("运行中"));
                    break;
                case cbct::DetectorState::Finished:
                    emit stateText(QStringLiteral("完成"));
                    break;
                case cbct::DetectorState::Error:
                    emit stateText(QStringLiteral("错误"));
                    break;
                }
            });
}

bool DetectorBridge::startWithRecipe(const cbct::Recipe &recipe, QString *error)
{
    // DetectorParams 需注册元类型才能作为 Q_ARG 跨线程传递
    qRegisterMetaType<cbct::DetectorParams>("cbct::DetectorParams");

    cbct::ClientCore::instance().initialize();
    auto &det = cbct::ClientCore::instance().detector();
    auto &box = cbct::ClientCore::instance().frameMailbox();
    box.clear(); // 上一轮残留帧丢掉

    const QString dir = cbct::paths::simulatedProjectionDir();

    bool ok = false;
    // BlockingQueued：UI 线程等到工作线程跑完 begin 再继续（可拿到成功/失败）
    const bool invoked = QMetaObject::invokeMethod(
        &det,
        "beginAcquisition",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, ok),
        Q_ARG(cbct::DetectorParams, recipe.detector),
        Q_ARG(QString, dir));

    if (!invoked) {
        if (error) {
            *error = QStringLiteral("无法调用采集线程 beginAcquisition");
        }
        return false;
    }
    if (!ok) {
        if (error) {
            *error = det.lastError();
        }
        return false;
    }

    emit started();
    return true;
}

void DetectorBridge::stop()
{
    auto &det = cbct::ClientCore::instance().detector();
    // Queued：不阻塞 UI；工作线程下个事件循环里 endAcquisition
    QMetaObject::invokeMethod(&det, "endAcquisition", Qt::QueuedConnection);
    emit stopped();
}

bool DetectorBridge::tryPopFrame(cbct::ProjectionFrame &out)
{
    return cbct::ClientCore::instance().frameMailbox().tryPop(out);
}

int DetectorBridge::pendingFrames() const
{
    return cbct::ClientCore::instance().frameMailbox().size();
}
