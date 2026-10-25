#include "common/log_service.h"

#include <QDir>
#include <QString>

#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace cbct {
namespace logging {

QString moduleName(Module m)
{
    switch (m) {
    case Module::App:
        return QStringLiteral("app");
    case Module::Ui:
        return QStringLiteral("ui");
    case Module::Recipe:
        return QStringLiteral("recipe");
    case Module::Device:
        return QStringLiteral("device");
    case Module::Db:
        return QStringLiteral("db");
    case Module::Core:
        return QStringLiteral("core");
    case Module::Bridge:
        return QStringLiteral("bridge");
    }
    return QStringLiteral("app");
}

LogService &LogService::instance()
{
    static LogService s;
    return s;
}

bool LogService::initialize(const QString &exeDir, const QString &level)
{
    if (m_ready) {
        return true;
    }

    // 日志目录：<可执行文件目录>/log
    m_logDir = exeDir + QStringLiteral("/log");
    if (!QDir().mkpath(m_logDir)) {
        return false;
    }

    spdlog::level::level_enum lv = spdlog::level::info;
    const QByteArray env = qgetenv("CBCT_LOG_LEVEL").toLower();
    const QString lvText = env.isEmpty() ? level : QString::fromLatin1(env);
    if (lvText == QLatin1String("trace")) {
        lv = spdlog::level::trace;
    } else if (lvText == QLatin1String("debug")) {
        lv = spdlog::level::debug;
    } else if (lvText == QLatin1String("warn") || lvText == QLatin1String("warning")) {
        lv = spdlog::level::warn;
    } else if (lvText == QLatin1String("error")) {
        lv = spdlog::level::err;
    }

    const QStringList modules = {
        QStringLiteral("app"),
        QStringLiteral("ui"),
        QStringLiteral("recipe"),
        QStringLiteral("device"),
        QStringLiteral("db"),
        QStringLiteral("core"),
        QStringLiteral("bridge"),
    };

    for (const QString &name : modules) {
        auto lg = makeLogger(name);
        if (!lg) {
            return false;
        }
        lg->set_level(lv);
        m_loggers[name.toStdString()] = lg;
    }

    m_ready = true;
    if (auto core = logger(Module::Core)) {
        core->info("日志系统初始化完成 dir={}", m_logDir.toStdString());
    }
    return true;
}

std::shared_ptr<spdlog::logger> LogService::makeLogger(const QString &name)
{
    try {
        // 每个模块单独文件；带时间戳文件名可后续加 rotation
        const std::string path = QStringLiteral("%1/%2.log").arg(m_logDir, name).toStdString();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        // 控制台也打一份，方便 VSCode/终端调试
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");

        auto lg = std::make_shared<spdlog::logger>(name.toStdString(),
                                                   spdlog::sinks_init_list{fileSink, consoleSink});
        // 每条 info 及以上都 flush，避免 _exit/崩溃时丢日志
        lg->flush_on(spdlog::level::info);
        spdlog::register_logger(lg);
        return lg;
    } catch (const std::exception &) {
        return nullptr;
    }
}

std::shared_ptr<spdlog::logger> LogService::logger(Module m)
{
    return logger(moduleName(m).toStdString());
}

std::shared_ptr<spdlog::logger> LogService::logger(const std::string &moduleNameUtf8)
{
    if (!m_ready) {
        return nullptr;
    }
    const auto it = m_loggers.find(moduleNameUtf8);
    if (it == m_loggers.end()) {
        return nullptr;
    }
    return it->second;
}

void LogService::flushAll()
{
    for (auto &kv : m_loggers) {
        if (kv.second) {
            kv.second->flush();
        }
    }
    spdlog::shutdown();
}

} // namespace logging
} // namespace cbct
