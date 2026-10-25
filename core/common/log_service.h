/**
 * @file log_service.h
 * @brief 基于 spdlog 的多模块日志服务
 *
 * - 日志目录：可执行文件旁的 log/（如 build/log/）
 * - 每个模块一个文件：app.log / ui.log / recipe.log / device.log / db.log / core.log / bridge.log
 * - 用法：
 *   CBCT_LOG_INFO("device", "开始采集 frames={}", n);
 *   CBCT_LOG_ERROR("db", err.toStdString());
 */
#ifndef CBCT_LOG_SERVICE_H
#define CBCT_LOG_SERVICE_H

#include <QString>
#include <memory>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace cbct {
namespace logging {

/// 支持的模块名（对应 log/<module>.log）
enum class Module {
    App,
    Ui,
    Recipe,
    Device,
    Db,
    Core,
    Bridge,
};

QString moduleName(Module m);

/**
 * @class LogService
 * @brief 进程内单例；initialize(exeDir) 后各模块 logger 可用
 */
class LogService
{
public:
    static LogService &instance();

    /**
     * @param exeDir 可执行文件所在目录
     * @param level  默认 info；环境变量 CBCT_LOG_LEVEL=debug/trace 可覆盖
     */
    bool initialize(const QString &exeDir, const QString &level = QStringLiteral("info"));

    std::shared_ptr<spdlog::logger> logger(Module m);
    std::shared_ptr<spdlog::logger> logger(const std::string &moduleNameUtf8);

    QString logDir() const { return m_logDir; }
    bool isReady() const { return m_ready; }
    void flushAll();

private:
    LogService() = default;
    std::shared_ptr<spdlog::logger> makeLogger(const QString &name);

    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
    QString m_logDir;
    bool m_ready = false;
};

} // namespace logging
} // namespace cbct

// 可变参数宏，支持 spdlog 格式化：CBCT_LOG_INFO("db", "id={}", id);
#define CBCT_LOG_TRACE(mod, ...) \
    do { \
        if (auto _cbct_l = ::cbct::logging::LogService::instance().logger(mod)) { \
            _cbct_l->trace(__VA_ARGS__); \
        } \
    } while (0)
#define CBCT_LOG_DEBUG(mod, ...) \
    do { \
        if (auto _cbct_l = ::cbct::logging::LogService::instance().logger(mod)) { \
            _cbct_l->debug(__VA_ARGS__); \
        } \
    } while (0)
#define CBCT_LOG_INFO(mod, ...) \
    do { \
        if (auto _cbct_l = ::cbct::logging::LogService::instance().logger(mod)) { \
            _cbct_l->info(__VA_ARGS__); \
        } \
    } while (0)
#define CBCT_LOG_WARN(mod, ...) \
    do { \
        if (auto _cbct_l = ::cbct::logging::LogService::instance().logger(mod)) { \
            _cbct_l->warn(__VA_ARGS__); \
        } \
    } while (0)
#define CBCT_LOG_ERROR(mod, ...) \
    do { \
        if (auto _cbct_l = ::cbct::logging::LogService::instance().logger(mod)) { \
            _cbct_l->error(__VA_ARGS__); \
        } \
    } while (0)

#endif // CBCT_LOG_SERVICE_H
