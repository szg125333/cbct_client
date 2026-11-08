#pragma once
// RTSTRUCT 轮廓解析（GDCM）。仅依赖 GDCM，不依赖 Qt/VTK。
// 对齐 DicomImageStudy/Utils/RtStructReader：只取 CLOSED_PLANAR 轮廓。

#include <memory>
#include <string>

#include "rt_struct_types.h"

namespace cbct::dicom {

class RtStructLoader {
public:
    /// 读取 RTSTRUCT 文件；失败返回 nullptr（原因写 dicom.log）
    static std::shared_ptr<RtStructureSet> load(const std::string& filePath);
};

}  // namespace cbct::dicom
