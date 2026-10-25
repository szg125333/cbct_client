#include "common/app_paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace cbct {
namespace paths {

const char kRecipeFileName[] = "recipes.json";
const char kDeviceConfigFileName[] = "device_config.json";
const char kLogFileName[] = "client.log";

QString appDataDir()
{
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

} // namespace paths
} // namespace cbct
