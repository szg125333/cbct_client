#pragma once

#include <string>
#include <array>
#include <vector>

/// @brief ROI 显示信息（纯数据，UI 层使用）
struct ROIDisplayInfo {
    int roiNumber = -1;
    std::string name;
    std::array<double, 3> color = { 0, 1, 0 };  // RGB 0~1
    bool visible = true;
};