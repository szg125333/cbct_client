#pragma once

#include "common/ViewTypes.h"
#include <array>
#include <string>
#include <map>

// 可显示的信息字段类型
enum class OverlayField {
    ViewType,
    WindowWidth,
    WindowLevel,
    SliceIndex,
    WorldPosition,      // X, Y, Z
    VoxelValue,         // 鼠标处像素值
    Custom              // 自定义字符串
};

struct RenderViewState {
    ViewType viewType = ViewType::None;
    double windowWidth = 0.0;
    double windowLevel = 0.0;
    int sliceIndex = -1;
    std::array<double, 3> worldPos = { 0.0, 0.0, 0.0 };
    double voxelValue = 0.0;
    std::map<std::string, std::string> customFields; // 自定义字段
};
