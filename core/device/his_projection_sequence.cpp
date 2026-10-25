#include "device/his_projection_sequence.h"

#include "common/log_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtMath>
#include <cstring>

namespace cbct {

namespace {
constexpr int kMinHeaderSize = 20;  ///< 小于此值视为头解析失败
constexpr int kMaxHeaderSize = 4096; ///< 读头时最多预读字节
}

bool HisProjectionSequence::open(const QString &directory, QString *error)
{
    close();

    QDir dir(directory);
    if (!dir.exists()) {
        CBCT_LOG_ERROR("device", "投影目录不存在: {}", directory.toStdString());
        if (error) {
            *error = QStringLiteral("投影数据目录不存在: %1").arg(directory);
        }
        return false;
    }

    // 文件名约定：image_rawA_00000.his
    const QRegularExpression re(QStringLiteral("^image_rawA_(\\d{5})\\.his$"),
                                QRegularExpression::CaseInsensitiveOption);
    QStringList names;
    const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.his"),
                                                    QDir::Files, QDir::Name); // Name 排序保证帧序
    for (const QFileInfo &fi : entries) {
        if (re.match(fi.fileName()).hasMatch()) {
            names << fi.absoluteFilePath();
        }
    }

    if (names.isEmpty()) {
        CBCT_LOG_ERROR("device", "目录中无 image_rawA_*.his: {}", directory.toStdString());
        if (error) {
            *error = QStringLiteral("目录中未找到 image_rawA_*.his: %1").arg(directory);
        }
        return false;
    }

    // 解析首帧头，得到宽高与头长
    QFile first(names.first());
    if (!first.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法打开: %1").arg(names.first());
        }
        return false;
    }
    const QByteArray header = first.read(kMaxHeaderSize);
    first.close();

    if (header.size() < kMinHeaderSize) {
        if (error) {
            *error = QStringLiteral("HIS 头过短: %1").arg(names.first());
        }
        return false;
    }

    // 小端 uint16 读头字段（catphan 实测布局）
    auto u16 = [&](int off) -> int {
        return static_cast<unsigned char>(header.at(off))
            | (static_cast<unsigned char>(header.at(off + 1)) << 8);
    };

    // off+4: 头长度；off+10: 位深字段；off+16/18: 宽/高
    m_headerSize = u16(4);
    m_bitsPerPixel = u16(10);
    m_width = u16(16);
    m_height = u16(18);

    // 头字段异常时：用「文件大小 - 100」反推正方形分辨率
    if (m_headerSize < kMinHeaderSize || m_headerSize > kMaxHeaderSize) {
        const qint64 payload = QFileInfo(names.first()).size() - 100;
        if (payload > 0 && payload % 2 == 0) {
            m_headerSize = 100;
            const qint64 pixels = payload / 2;
            const int side = static_cast<int>(qSqrt(static_cast<double>(pixels)));
            if (side * side == pixels) {
                m_width = side;
                m_height = side;
            } else {
                m_width = 1440;
                m_height = 1440;
            }
        }
    }

    if (m_width <= 0 || m_height <= 0) {
        if (error) {
            *error = QStringLiteral("无法解析 HIS 尺寸");
        }
        return false;
    }

    m_directory = QFileInfo(directory).absoluteFilePath();
    m_files = names;
    loadAngles();
    CBCT_LOG_INFO("device", "HIS 序列已打开 frames={} size={}x{} header={}",
                  static_cast<int>(m_files.size()), m_width, m_height, m_headerSize);
    return true;
}

void HisProjectionSequence::close()
{
    m_directory.clear();
    m_files.clear();
    m_width = m_height = m_headerSize = 0;
    m_bitsPerPixel = 16;
    m_angles.clear();
}

bool HisProjectionSequence::readFrame(int index, QVector<uint16_t> &buffer, QString *error) const
{
    if (index < 0 || index >= m_files.size()) {
        if (error) {
            *error = QStringLiteral("帧索引越界: %1").arg(index);
        }
        return false;
    }

    QFile file(m_files.at(index));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法打开帧: %1").arg(m_files.at(index));
        }
        return false;
    }

    // 跳过文件头，只读像素区
    if (!file.seek(m_headerSize)) {
        if (error) {
            *error = QStringLiteral("seek 失败: %1").arg(m_files.at(index));
        }
        return false;
    }

    const qint64 bytesNeeded = static_cast<qint64>(m_width) * m_height * 2; // uint16
    const QByteArray raw = file.read(bytesNeeded);
    if (raw.size() != bytesNeeded) {
        if (error) {
            *error = QStringLiteral("帧数据不完整: %1").arg(m_files.at(index));
        }
        return false;
    }

    const int pixels = m_width * m_height;
    buffer.resize(pixels);
    const auto *src = reinterpret_cast<const uint16_t *>(raw.constData());
    std::memcpy(buffer.data(), src, static_cast<size_t>(pixels) * sizeof(uint16_t));
    return true;
}

double HisProjectionSequence::gantryAngleDeg(int index) const
{
    if (index >= 0 && index < m_angles.size()) {
        return m_angles.at(index);
    }
    if (m_files.isEmpty()) {
        return 0.0;
    }
    // 无角度文件时按均匀 360° 均分
    return index * (360.0 / m_files.size());
}

bool HisProjectionSequence::loadAngles()
{
    m_angles.clear();

    // 同目录下的采集信息 JSON（RTK/设备导出）
    const QString jsonPath = m_directory + QStringLiteral("/Plate1AcquisitionInformationXml.json");
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
    // Image[] 每项含 GantryAngle 字段
    const QJsonArray images = doc.object().value(QStringLiteral("Image")).toArray();
    for (const QJsonValue &v : images) {
        m_angles.append(v.toObject().value(QStringLiteral("GantryAngle")).toDouble());
    }
    return !m_angles.isEmpty();
}

} // namespace cbct
