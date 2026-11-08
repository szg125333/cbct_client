#pragma once

/**
 * @brief 自由手绘 ROI 统计结果
 *
 * 对应图片中的统计项：
 *   Area            → 面积（mm²，由 spacing 换算）
 *   Perimeter       → 周长（mm，沿轮廓线段长度累加）
 *   Mean Pixel Value→ 均值（HU）
 *   Standard Deviation → 标准差
 *   Min Pixel Value → 最小值（HU）
 *   Max Pixel Value → 最大值（HU）
 *   Number Of Pixels→ 区域内像素总数
 */
struct FreehandStats {
    double area = 0.0;   ///< 面积（mm²）
    double perimeter = 0.0;   ///< 周长（mm）
    double mean = 0.0;   ///< 均值（HU）
    double stdDev = 0.0;   ///< 标准差
    double minVal = 0.0;   ///< 最小像素值（HU）
    double maxVal = 0.0;   ///< 最大像素值（HU）
    int    pixelCount = 0;     ///< 区域内像素数量

    bool IsValid() const { return pixelCount > 0; }
};
