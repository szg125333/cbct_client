#include "common/app_paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace cbct {
namespace paths {

const char kRecipeFileName[] = "recipes.json";
const char kDeviceConfigFileName[] = "device_config.json";
const char kLogFileName[] = "client.log";
const char kDefaultSimulatedProjectionDir[] = "/mnt/hgfs/share/catphan";

namespace {
/// 运行时覆盖的投影目录；空表示未设置
QString g_simulatedProjectionDir;
}

QString appDataDir()
{
    // 例如 /home/用户/.local/share/CBCT/CBCT Client
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString recipeStoragePath()
{
    return appDataDir() + QLatin1Char('/') + QLatin1String(kRecipeFileName);
}

QString deviceConfigPath()
{
    return appDataDir() + QLatin1Char('/') + QLatin1String(kDeviceConfigFileName);
}

QString logFilePath()
{
    return appDataDir() + QLatin1Char('/') + QLatin1String(kLogFileName);
}

bool ensureDirFor(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (info.absolutePath().isEmpty()) {
        return false;
    }
    return QDir().mkpath(info.absolutePath());
}

QString simulatedProjectionDir()
{
    if (!g_simulatedProjectionDir.isEmpty()) {
        return g_simulatedProjectionDir;
    }
    const QByteArray env = qgetenv("CBCT_SIM_PROJ_DIR");
    if (!env.isEmpty()) {
        return QString::fromLocal8Bit(env);
    }
    return QString::fromLatin1(kDefaultSimulatedProjectionDir);
}

void setSimulatedProjectionDir(const QString &dir)
{
    g_simulatedProjectionDir = dir;
}

} // namespace paths
} // namespace cbct
