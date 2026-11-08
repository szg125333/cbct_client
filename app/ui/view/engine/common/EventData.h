#pragma once

#include "ViewTypes.h"
#include <string>

/**
 * @brief 用户交互事件类型
 *
 * VTK 原生事件经过 VtkViewRenderer::VtkEventCallback 转换后，
 * 统一以此枚举表示，屏蔽 VTK 细节。
 */
enum class EventType {
    WheelForward,   ///< 滚轮向前（切片++）
    WheelBackward,  ///< 滚轮向后（切片--）
    LeftPress,      ///< 左键按下
    LeftMove,       ///< 左键拖拽（鼠标移动且左键按下）
    LeftRelease,    ///< 左键释放
    RightPress,     ///< 右键按下
    RightRelease,   ///< 右键释放
    KeyPress,       ///< 键盘按下
    KeyRelease,     ///< 键盘释放
};

/**
 * @brief 交互事件附属数据
 *
 * 由 VtkViewRenderer 填充，传递给 IInteractionStrategy::HandleEvent。
 */
struct EventData {
    /// 鼠标屏幕坐标（像素，原点在左下角，与 VTK 约定一致）
    int mousePosX = 0;
    int mousePosY = 0;

    /// 修饰键状态
    bool ctrlPressed = false;
    bool shiftPressed = false;
    bool altPressed = false;

    /// 键盘符号（仅 KeyPress / KeyRelease 事件有效，如 "Escape"、"Delete"）
    std::string keySym;
};
