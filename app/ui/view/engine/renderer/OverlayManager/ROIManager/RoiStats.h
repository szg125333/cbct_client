#pragma once

#include <array>

/**
 * @brief ROI 区域统计结果
 *
 * 存储矩形 ROI 内图像像素（HU 值）的统计信息。
 * 由 SimpleROIManager::ComputeStats() 填充，
 * 显示在矩形框正下方的悬浮标签中。
 */
struct RoiStats {
    double mean = 0.0;    ///< 均值（HU）
    double stdDev = 0.0;    ///< 标准差
    double minVal = 0.0;    ///< 最小值（HU）
    double maxVal = 0.0;    ///< 最大值（HU）
    double area = 0.0;    ///< 面积（mm²）
    int    pixelCount = 0;      ///< 参与统计的像素数量

    /// @brief 统计结果是否有效（像素数量 > 0）
    bool IsValid() const { return pixelCount > 0; }
};