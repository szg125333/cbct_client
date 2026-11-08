// ================================================================
//  Common/ViewTypes.h
//  切片视图方向枚举
// ================================================================
#pragma once

/**
 * @brief 切片视图方向
 *
 * 与 VTK vtkImageViewer2 的切片方向一一对应：
 *   Axial    ↔ SetSliceOrientationToXY()
 *   Sagittal ↔ SetSliceOrientationToYZ()
 *   Coronal  ↔ SetSliceOrientationToXZ()
 */
enum class ViewType {
    Axial = 0,   ///< 轴状位（水平面，XY 平面）
    Sagittal = 1,   ///< 矢状位（纵向左右分割面，YZ 平面）
    Coronal = 2,   ///< 冠状位（纵向前后分割面，XZ 平面）
    None = 3,   ///< 未指定（初始默认值）
};
