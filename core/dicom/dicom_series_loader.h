#pragma once
// GDCM 实现的 DICOM 序列读取器（替代 DicomImageStudy 中基于 ITK 的 DicomLoader）
// 仅依赖 GDCM；错误用枚举返回，不用异常跨 core 边界

#include <string>
#include <vector>

#include "dicom_types.h"

namespace cbct::dicom {

class DicomSeriesLoader {
public:
    /// 目录内一个已识别的序列（已过滤 RTSTRUCT/RTPLAN/RTDOSE/REG）
    struct SeriesInfo {
        std::string seriesUID;
        std::string modality;
        std::string description;
        int fileCount = 0;
        std::vector<std::string> files;  // 已按空间位置（IPP+IOP）排序
    };

    /// 枚举目录下所有可成体的序列
    static std::vector<SeriesInfo> enumerateSeries(const std::string& directory);

    /// 打开目录：取 fileCount 最大的序列并读像素（与原 ITK 逻辑一致）
    static DicomVolumeData loadDirectory(const std::string& directory,
                                         DicomLoadError* err = nullptr);

    /// 读指定序列
    static DicomVolumeData loadSeries(const SeriesInfo& series,
                                      DicomLoadError* err = nullptr);

    /// 单文件 / 多文件；多文件必须同 SeriesInstanceUID，否则 MixedSeries
    static DicomVolumeData loadFiles(const std::vector<std::string>& files,
                                     DicomLoadError* err = nullptr);

    /// 是否 RTSTRUCT（打开对话框预过滤 / 二期）
    static bool isRtStruct(const std::string& path);

private:
    static DicomVolumeData loadSortedFiles(const std::vector<std::string>& files,
                                           DicomLoadError* err);
};

}  // namespace cbct::dicom
