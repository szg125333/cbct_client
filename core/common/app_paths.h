/**
 * @file app_paths.h
 * @brief 全局路径约定：应用数据目录、配方 JSON、模拟投影目录等
 *
 * 所有“文件放哪”集中在这里，避免魔法字符串散落各处。
 * 以后新增配置/日志文件，也在此增加 xxxPath()。
 */
#ifndef CBCT_APP_PATHS_H
#define CBCT_APP_PATHS_H

#include <QString>

namespace cbct {
namespace paths {

/// 默认文件名常量（相对应用数据目录）
extern const char kRecipeFileName[];             ///< "recipes.json"
extern const char kDeviceConfigFileName[];       ///< "device_config.json"（预留）
extern const char kLogFileName[];                ///< "client.log"（预留）
extern const char kDefaultSimulatedProjectionDir[]; ///< 模拟投影默认目录

/// 应用本地数据根目录（QStandardPaths::AppLocalDataLocation）
/// 依赖 main 里设置的组织名 "CBCT" + 应用名 "CBCT Client"
QString appDataDir();

/// 完整路径拼接：appDataDir() + "/" + 文件名
QString recipeStoragePath();
QString deviceConfigPath();
QString logFilePath();

/// 写文件前确保父目录存在
bool ensureDirFor(const QString &filePath);

/**
 * 模拟投影数据目录（HIS 序列）。
 * 优先级：
 *   1) setSimulatedProjectionDir() 运行时覆盖
 *   2) 环境变量 CBCT_SIM_PROJ_DIR
 *   3) kDefaultSimulatedProjectionDir（/mnt/hgfs/share/catphan）
 */
QString simulatedProjectionDir();
void setSimulatedProjectionDir(const QString &dir);

} // namespace paths
} // namespace cbct

#endif // CBCT_APP_PATHS_H
