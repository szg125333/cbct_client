#include "common/json_file_io.h"

#include "common/log_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace cbct {

bool JsonFileIo::read(const QString &path, QJsonObject &root, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        CBCT_LOG_WARN("core", "JSON 打开失败: {}", path.toStdString());
        if (error) {
            *error = QStringLiteral("无法打开文件: %1").arg(path);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("JSON 解析失败: %1").arg(parseError.errorString());
        }
        return false;
    }
    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON 根节点不是对象");
        }
        return false;
    }

    root = doc.object();
    return true;
}

bool JsonFileIo::write(const QString &path, const QJsonObject &root, QString *error)
{
    const QFileInfo info(path);
    if (!info.absolutePath().isEmpty() && !QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("无法创建目录: %1").arg(info.absolutePath());
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法写入文件: %1").arg(path);
        }
        return false;
    }

    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        if (error) {
            *error = QStringLiteral("写入数据不完整: %1").arg(path);
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        CBCT_LOG_ERROR("core", "JSON 提交失败: {}", path.toStdString());
        if (error) {
            *error = QStringLiteral("提交文件失败: %1").arg(path);
        }
        return false;
    }
    return true;
}

} // namespace cbct
