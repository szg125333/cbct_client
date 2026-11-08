#pragma once

#include "renderer/OverlayManager/IOverlayFeature.h"

#include <array>
#include <vector>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkImageViewer2;
class vtkActor;
class vtkFollower;
class vtkVectorText;
class vtkCoordinate;

/**
 * @brief 双线独立刻度尺 Overlay Feature
 *
 * 刻度规格：
 *   每 1mm 一个刻度线（短刻 kTickShort，5mm 中刻 kTickMid，10mm 长刻 kTickLong）
 *   只有一个标签："1mm"，放在交叉点旁，标注最小刻度单位
 *   标签字体大小由 kLabelScale 控制（默认 3.0mm）
 *
 * 使用方式：
 *   PlaceAtImageCenter() — 模式激活时（OnActivated）主动调用
 *   HitTest()            — LeftPress 时判断命中哪条线
 *   MoveHorizontalLine() / MoveVerticalLine() — LeftMove 时调用
 */
class SimpleRulerLineManager : public IOverlayFeature {
public:
    enum class HitLine { None, Horizontal, Vertical };

    SimpleRulerLineManager();
    ~SimpleRulerLineManager() override;

    // IOverlayFeature
    void Initialize(vtkRenderer* overlayRenderer) override;
    void SetVisible(bool visible)                 override;
    void SetColor(double r, double g, double b)   override;
    void Shutdown()                               override;
    void OnSliceChanged(vtkImageViewer2* viewer,
        int slice,
        ViewType viewType)         override;

    // 放置
    void PlaceAtImageCenter(vtkImageViewer2* viewer,
        ViewType viewType, int slice);

    // 拖动
    HitLine HitTest(int screenX, int screenY) const;
    void    MoveHorizontalLine(const std::array<double, 3>& worldDelta);
    void    MoveVerticalLine(const std::array<double, 3>& worldDelta);

    bool   IsPlaced()         const { return m_isPlaced; }
    double GetHLinePosition() const { return m_hLinePos; }
    double GetVLinePosition() const { return m_vLinePos; }

private:
    static void GetPlaneAxes(ViewType vt, int& ax0, int& ax1, int& axF);

    void RebuildHLine();
    void RebuildVLine();

    /**
     * @brief 生成一条参考线的所有图元
     *
     * @param isHorizontal  true=水平线(沿ax0), false=垂直线(沿ax1)
     * @param linePos       参考线在垂直方向的世界坐标
     * @param outActor      [out] 主线+刻度合并Actor
     * @param outLabel      [out] "1mm"标签的Follower（只有一个）
     * @param outLabelText  [out] 对应的vtkVectorText
     */
    void BuildLine(bool isHorizontal,
        double linePos,
        vtkSmartPointer<vtkActor>& outActor,
        vtkSmartPointer<vtkFollower>& outLabel,
        vtkSmartPointer<vtkVectorText>& outLabelText);

    void ClearHLine();
    void ClearVLine();

    bool WorldToScreen(const std::array<double, 3>& world,
        double& outX, double& outY) const;

    // 成员
    vtkRenderer* m_overlayRenderer = nullptr;
    bool         m_initialized = false;
    bool         m_visible = true;
    bool         m_isPlaced = false;

    ViewType m_viewType = ViewType::None;
    int      m_slice = 0;
    double   m_axFVal = 0.0;
    double   m_imageMin[3] = {};
    double   m_imageMax[3] = {};

    double m_hLinePos = 0.0;   // 水平线在 ax1 上的位置
    double m_vLinePos = 0.0;   // 垂直线在 ax0 上的位置

    // 水平线图元
    vtkSmartPointer<vtkActor>    m_hActor;
    vtkSmartPointer<vtkFollower> m_hLabel;
    vtkSmartPointer<vtkVectorText> m_hLabelText;

    // 垂直线图元
    vtkSmartPointer<vtkActor>    m_vActor;
    vtkSmartPointer<vtkFollower> m_vLabel;
    vtkSmartPointer<vtkVectorText> m_vLabelText;

    static constexpr double kHitTolerancePx = 8.0;

    // 刻度高度（mm）
    static constexpr double kTickShort = 1.5;   // 1mm 处
    static constexpr double kTickMid = 3.0;   // 5mm 处
    static constexpr double kTickLong = 5.0;   // 10mm 处

    // ★ 字体大小：修改 kLabelScale 调整（单位 mm，世界坐标）
    //   3.0 ≈ 屏幕 5px（小）
    //   5.0 ≈ 屏幕 8px（中）
    //   8.0 ≈ 屏幕 12px（大，项目其他地方的默认值）
    static constexpr double kLabelScale = 3.0;
    static constexpr double kLabelOffset = 2.0;  // 标签距刻度线的偏移（mm）
};
