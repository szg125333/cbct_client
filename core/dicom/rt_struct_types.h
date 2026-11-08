#pragma once
// RT Structure Set（RTSTRUCT）解析结果：纯 C++ + STL，不依赖 Qt/VTK（core 分层硬约束）
// 显示侧另有 app/ui/view/engine/utils/RTStructureData.h（VTK 引擎用），由 dicom_bridge 转换。

#include <array>
#include <map>
#include <string>
#include <vector>

namespace cbct::dicom {

/// 单条轮廓线（一个切片上的闭合轮廓，世界坐标 mm / DICOM LPS）
struct RtContour {
    double z = 0.0;
    std::vector<std::array<double, 3>> points;
};

/// 一个 ROI：编号、名称、显示颜色、所有轮廓
struct RtStructureRoi {
    int roiNumber = -1;
    std::string name;
    std::array<double, 3> color = {1.0, 0.0, 0.0};  // 归一化 0~1
    std::vector<RtContour> contours;
};

/// 整个 RT Structure Set
struct RtStructureSet {
    std::string label;
    std::string name;
    std::map<int, RtStructureRoi> rois;  // key = ROI Number
};

}  // namespace cbct::dicom
