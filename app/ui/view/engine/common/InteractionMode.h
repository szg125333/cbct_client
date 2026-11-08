#pragma once
#ifndef INTERACTION_MODE_H
#define INTERACTION_MODE_H

#include <string>
#include <stdexcept>

/// 定义所有交互模式
enum class InteractionMode :int{
    Normal = 0,           // 普通浏览 / 拾取
    Checkboard,           // 棋盘格对比模式
    ImageDrag,           // 手动平移/旋转（移动对象）
    DistanceMeasure,      // 距离测量工具
    AngleMeasure,         // 角度测量工具
    ContourMeasure,       // 轮廓测量 / 手绘轮廓
    RegistrationROI,      // 配准 ROI（盒子）模式
    FreehandROI,          // 手工不规则轮廓
    CrosshairRuler,          // 手工不规则轮廓
    SliceScroll,       
    None                  // 无交互（占位，用于计数或禁用）
};

/// 定义模式数量（用于校验或循环）
constexpr int InteractionModeCount = static_cast<int>(InteractionMode::None);

/// 将模式转换为可读字符串（用于日志或 UI 显示）
inline std::string ToString(InteractionMode m) {
    switch (m) {
    case InteractionMode::Normal: return "Normal";
    case InteractionMode::Checkboard: return "Checkboard";
    case InteractionMode::ImageDrag: return "ImageDrag";
    case InteractionMode::DistanceMeasure: return "DistanceMeasure";
    case InteractionMode::AngleMeasure: return "AngleMeasure";
    case InteractionMode::ContourMeasure: return "ContourMeasure";
    case InteractionMode::RegistrationROI: return "RegistrationROI";
    case InteractionMode::FreehandROI: return "FreehandROI";
    case InteractionMode::CrosshairRuler: return "CrosshairRuler";
    case InteractionMode::None: return "None";
    default: return "Unknown";
    }
}

/// 从整数安全转换为 InteractionMode（带边界检查）
inline InteractionMode ModeFromInt(int v) {
    if (v < 0 || v >= InteractionModeCount) throw std::out_of_range("Invalid InteractionMode value");
    return static_cast<InteractionMode>(v);
}

#endif // INTERACTION_MODE_H
