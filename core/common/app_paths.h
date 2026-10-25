#ifndef CBCT_APP_PATHS_H
#define CBCT_APP_PATHS_H

#include <QString>

namespace cbct {
namespace paths {

/// 默认 JSON 文件名（可扩展：设备配置、日志等各自文件名）
extern const char kRecipeFileName[];
extern const char kDeviceConfigFileName[];
extern const char kLogFileName[];

/// 应用本地数据根目录（随组织名/应用名变化）
QString appDataDir();

/// 可写文件完整路径：appDataDir() + "/" + fileName
QString recipeStoragePath();
QString deviceConfigPath();
QString logFilePath();

/// 保证目录存在
bool ensureDirFor(const QString &filePath);

} // namespace paths
} // namespace cbct

#endif // CBCT_APP_PATHS_H
