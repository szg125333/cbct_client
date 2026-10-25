#ifndef CBCT_JSON_FILE_IO_H
#define CBCT_JSON_FILE_IO_H

#include <QJsonObject>
#include <QString>

namespace cbct {

/// 通用 JSON 文件读写（可复用于任意 QJsonObject 根对象）
class JsonFileIo
{
public:
    static bool read(const QString &path, QJsonObject &root, QString *error = nullptr);
    static bool write(const QString &path, const QJsonObject &root, QString *error = nullptr);
};

} // namespace cbct

#endif // CBCT_JSON_FILE_IO_H
