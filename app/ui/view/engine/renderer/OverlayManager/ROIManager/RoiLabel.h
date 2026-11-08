#pragma once

#include <array>
#include <string>
#include <vector>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkCamera;
class vtkFollower;
class vtkVectorText;

/**
 * @brief ROI 多行统计标签组件
 *
 * vtkVectorText 不支持 '\n' 换行，本类通过为每行文字单独创建一个
 * vtkFollower 来实现多行效果。
 *
 * 每行在世界坐标中沿"下方"方向依次偏移 kLineSpacingMm，
 * 所有行共用同一 vtkCamera，始终朝向屏幕。
 *
 * 使用方式：
 *   RoiLabel label;
 *   label.Initialize(renderer);
 *   label.SetLines({"Mean: 45.2", "SD: 12.1", ...});
 *   label.SetAnchor(anchorPos, ViewType::Axial);   // 第一行位置
 *   label.SetVisible(true);
 *
 * 更新时只需再次调用 SetLines() + SetAnchor()，无需重建。
 */
class RoiLabel {
public:
    RoiLabel() = default;
    ~RoiLabel();

    // ----------------------------------------------------------------
    //  生命周期
    // ----------------------------------------------------------------

    /// @brief 初始化（绑定渲染器），只调用一次
    void Initialize(vtkRenderer* renderer);

    /// @brief 从渲染器移除所有行的 Actor，释放资源
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

    // ----------------------------------------------------------------
    //  内容与位置
    // ----------------------------------------------------------------

    /**
     * @brief 设置每行文字内容
     *
     * 若行数增加则自动追加新 Follower；
     * 若行数减少则隐藏多余行（不销毁，下次可复用）。
     */
    void SetLines(const std::vector<std::string>& lines);

    /**
     * @brief 设置标签第一行的锚点位置，并根据视图方向依次向下排列各行
     *
     * @param anchor    第一行世界坐标（矩形下边中点偏移后的位置）
     * @param viewType  视图方向（决定"向下"是哪个轴的负方向）
     */
    void SetAnchor(const std::array<double, 3>& anchor, int viewType);

    /// @brief 控制所有行的可见性
    void SetVisible(bool visible);

    // ----------------------------------------------------------------
    //  参数
    // ----------------------------------------------------------------

    /// 文字缩放比例（世界坐标单位，默认 6.5）
    double scale = 6.5;

    /// 行间距（mm，默认 8.0）
    double lineSpacingMm = 8.0;

private:
    /// @brief 确保 m_rows 中至少有 n 行（不足则追加）
    void EnsureRowCount(int n);

    struct LabelRow {
        vtkSmartPointer<vtkVectorText> textSrc;
        vtkSmartPointer<vtkFollower>   follower;
    };

    vtkRenderer* m_renderer = nullptr;
    bool         m_initialized = false;

    std::vector<LabelRow> m_rows;   ///< 每行一个 Follower
};