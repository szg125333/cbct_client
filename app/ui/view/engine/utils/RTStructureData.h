#pragma once

#include <string>
#include <vector>
#include <map>
#include <array>

/// @brief 单条轮廓线（一个切片上的闭合轮廓）
struct RTContour {
    double z = 0.0;                              // 该轮廓所在的 Z 坐标（世界坐标）
    std::vector<std::array<double, 3>> points;   // 轮廓上的所有点 (x, y, z)
};

/// @brief 一个 ROI（感兴趣区域），包含名称、颜色和多条轮廓
struct RTStructureROI {
    int roiNumber = -1;
    std::string name;
    std::array<double, 3> color = { 1.0, 0.0, 0.0 }; // 显示颜色 (R,G,B)，归一化到 0~1
    std::vector<RTContour> contours;                   // 该 ROI 的所有轮廓切片
};

/// @brief RT Structure 数据集，包含所有 ROI
struct RTStructureData {
    std::string label;
    std::string name;
    std::map<int, RTStructureROI> rois; // key = ROI Number
};