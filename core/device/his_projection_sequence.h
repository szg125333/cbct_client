/**
 * @file his_projection_sequence.h
 * @brief 目录内多帧单文件 HIS 投影序列读取器
 *
 * 数据形态（catphan 实测）：
 *   /…/catphan/image_rawA_00000.his … image_rawA_00719.his  共 720 帧
 *   每文件：100 字节头 + 1440×1440 uint16 像素
 *
 * 内存策略：不整目录载入；readFrame(index) 只打开并读取当前一帧。
 */
#ifndef CBCT_HIS_PROJECTION_SEQUENCE_H
#define CBCT_HIS_PROJECTION_SEQUENCE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace cbct {

/**
 * @class HisProjectionSequence
 * @brief 扫描目录、解析首帧头、按索引读帧
 */
class HisProjectionSequence
{
public:
    HisProjectionSequence() = default;

    /**
     * @brief 扫描 directory 下 image_rawA_*.his，解析尺寸
     * @param directory 含投影文件的目录
     * @param error 可选，失败时写入原因
     */
    bool open(const QString &directory, QString *error = nullptr);
    void close();

    bool isOpen() const { return !m_files.isEmpty(); }
    int frameCount() const { return m_files.size(); }  ///< 帧数 = 文件个数
    int width() const { return m_width; }              ///< 像素宽
    int height() const { return m_height; }            ///< 像素高
    int headerSize() const { return m_headerSize; }    ///< 单帧文件头字节数
    QString directory() const { return m_directory; }

    /**
     * @brief 读第 index 帧到 buffer（元素个数 = width*height）
     * @note 仅分配/读取当前帧，不会缓存其它帧
     */
    bool readFrame(int index, QVector<uint16_t> &buffer, QString *error = nullptr) const;

    /// 龙门架角：优先用 JSON；否则 index * (360/frameCount)
    double gantryAngleDeg(int index) const;

private:
    bool loadAngles(); ///< 尝试读 Plate1AcquisitionInformationXml.json

    QString m_directory;        ///< 序列根目录绝对路径
    QStringList m_files;        ///< 按文件名排序的 .his 绝对路径列表
    int m_width = 0;            ///< 图像宽 (px)
    int m_height = 0;           ///< 图像高 (px)
    int m_headerSize = 0;       ///< 帧文件头大小（catphan=100）
    int m_bitsPerPixel = 16;    ///< 头中记录的位深（当前按 16bit 处理）
    QVector<double> m_angles;   ///< 各帧 GantryAngle（可为空）
};

} // namespace cbct

#endif // CBCT_HIS_PROJECTION_SEQUENCE_H
