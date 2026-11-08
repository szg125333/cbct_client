#pragma once

#include <array>
#include <cmath>
#include <vtkCamera.h>

/**
 * @brief 视口坐标工具：屏幕像素增量 → 世界坐标增量
 *
 * ============================================================
 *  根因分析：为什么之前的移动方向会反转
 * ============================================================
 *
 *  VTK interactor->GetEventPosition() 使用 OpenGL 坐标系：
 *    原点 = 窗口左下角，X 向右，Y 向上（↑ 为正）
 *
 *  之前代码写的是：
 *    const double wdY = -pixDy * worldPerPixelY;  // 取了反
 *
 *  这个负号的来历：作者假设 pixDy 是"屏幕向下为正"（Qt/Windows 坐标系），
 *  为了让鼠标下滑→图像下移，需要取反后乘以 viewUp。
 *
 *  但 VTK GetEventPosition 给的 Y 是 OpenGL Y（向上为正），
 *  鼠标向下移动时 OpenGL Y 减小（pixDy < 0），
 *  取反后 -pixDy > 0，乘以 viewUp（向上方向），结果是图像向上移动。
 *  与预期相反，出现"下滑图像上走"的 bug。
 *
 *  修正：pixDy 来自 OpenGL Y（向上为正），直接乘以 viewUp，不取反：
 *    鼠标上移 → pixDy > 0 → worldDelta 沿 viewUp 正方向 → 图像上移 ✓
 *    鼠标下移 → pixDy < 0 → worldDelta 沿 viewUp 负方向 → 图像下移 ✓
 *
 *  体位兼容性：
 *    FFS 体位 ViewUp = (0, -1, 0)，图像上下翻转，但 OpenGL Y 方向不变，
 *    pixDy 直接乘以 ViewUp 后方向自动正确，无需额外处理。
 *
 * ============================================================
 *  所有需要修改的位置（项目内）
 * ============================================================
 *
 *  1. ImageDragStrategy.cpp   ← LeftMove 中的 wdY 计算
 *  2. CrosshairRulerStrategy.cpp ← LeftMove 中的 wdY 计算
 *  3. RulerLineStrategy.cpp   ← PixelDeltaToWorld 辅助函数
 *  4. FreehandROIStrategy.cpp ← LeftMove 中的 delta 计算（如果有像素换算）
 *
 *  统一做法：所有策略都改为调用本文件的 ViewportUtils::PixelDeltaToWorld，
 *  不再各自重复这段逻辑。
 */
namespace ViewportUtils {

    /**
     * @brief 将屏幕像素增量换算为世界坐标增量（正交投影）
     *
     * @param pixDx     X 方向像素增量（向右为正）
     * @param pixDy     Y 方向像素增量（向上为正，来自 VTK GetEventPosition）
     * @param camera    当前视图的正交相机
     * @param viewportW 视口宽度（像素）
     * @param viewportH 视口高度（像素）
     * @return          世界坐标增量 {dx, dy, dz}（mm）
     */
    inline std::array<double, 3> PixelDeltaToWorld(
        int        pixDx,
        int        pixDy,
        vtkCamera* camera,
        int        viewportW,
        int        viewportH)
    {
        if (!camera || viewportW <= 0 || viewportH <= 0)
            return { 0.0, 0.0, 0.0 };

        const double parallelScale = camera->GetParallelScale();
        const double worldPerPixelY = (2.0 * parallelScale) / viewportH;
        const double aspect = static_cast<double>(viewportW) / viewportH;
        const double worldPerPixelX = worldPerPixelY * aspect;

        double viewUp[3], viewDir[3];
        camera->GetViewUp(viewUp);
        camera->GetDirectionOfProjection(viewDir);

        double rightVec[3];
        rightVec[0] = viewDir[1] * viewUp[2] - viewDir[2] * viewUp[1];
        rightVec[1] = viewDir[2] * viewUp[0] - viewDir[0] * viewUp[2];
        rightVec[2] = viewDir[0] * viewUp[1] - viewDir[1] * viewUp[0];

        double lenR = std::sqrt(rightVec[0] * rightVec[0] +
            rightVec[1] * rightVec[1] +
            rightVec[2] * rightVec[2]);
        if (lenR < 1e-9) return { 0.0, 0.0, 0.0 };
        rightVec[0] /= lenR; rightVec[1] /= lenR; rightVec[2] /= lenR;

        // ✓ 不取反：VTK OpenGL Y 向上为正，ViewUp 也是向上，符号一致
        const double wdX = static_cast<double>(pixDx) * worldPerPixelX;
        const double wdY = static_cast<double>(pixDy) * worldPerPixelY;

        return {
            rightVec[0] * wdX + viewUp[0] * wdY,
            rightVec[1] * wdX + viewUp[1] * wdY,
            rightVec[2] * wdX + viewUp[2] * wdY,
        };
    }

} // namespace ViewportUtils
