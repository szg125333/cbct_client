#pragma once

#include "renderer/OverlayManager/IOverlayFeature.h"
#include "FreehandStats.h"
#include "renderer/OverlayManager/ROIManager/RoiLabel.h"

#include <array>
#include <vector>
#include <unordered_map>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkImageViewer2;
class vtkActor;
class vtkPolyData;
class vtkPoints;
class vtkCellArray;
class vtkPolyDataMapper;
class vtkCoordinate;

// ----------------------------------------------------------------
//  命中测试结果（用于拖动）
// ----------------------------------------------------------------

/**
 * @brief 手绘 ROI 命中类型
 */
enum class FreehandHitType {
    None,       ///< 未命中
    Outline,    ///< 命中轮廓线或内部（整体平移）
};

struct FreehandHitResult {
    int             roiId = -1;
    FreehandHitType hitType = FreehandHitType::None;
};

// ----------------------------------------------------------------
//  SimpleFreehandROIManager
// ----------------------------------------------------------------

/**
 * @brief 自由手绘 ROI Overlay Feature
 *
 * 修正记录：
 *   v2 — 修复 CreateOutlineActor / CreateFillActor 中点坐标硬编码
 *          XY 平面的问题，改为由 viewType 驱动轴选择，三视图均可用。
 *      — 新增拖动支持：HitTest() 命中已完成 ROI 后可整体平移。
 *
 * 功能：
 *   - 鼠标拖动绘制不规则轮廓（三视图均支持）
 *   - 释放后闭合轮廓 + 半透明填充 + 多行统计标签（框正下方）
 *   - 统计：Area / Perimeter / Mean / SD / Min / Max / Pixels
 *   - 支持整体拖动已完成的 ROI（平移轮廓 + 实时重算统计）
 *   - 切片切换时坐标自动投影到新切片
 */
class SimpleFreehandROIManager : public IOverlayFeature {
public:
    SimpleFreehandROIManager();
    ~SimpleFreehandROIManager() override;

    // ----------------------------------------------------------------
    //  IOverlayFeature
    // ----------------------------------------------------------------
    void Initialize(vtkRenderer* overlayRenderer)           override;
    void SetVisible(bool visible)                           override;
    void SetColor(double r, double g, double b)             override;
    void Shutdown()                                         override;
    void OnSliceChanged(vtkImageViewer2* viewer,
        int slice,
        ViewType viewType)                  override;

    // ----------------------------------------------------------------
    //  绘制接口
    // ----------------------------------------------------------------
    void BeginFreehand(const std::array<double, 3>& startWorld,
        ViewType viewType, int slice);
    void AddPoint(const std::array<double, 3>& worldPoint);
    void CommitFreehand(vtkImageViewer2* viewer);
    void CancelFreehand();

    // ----------------------------------------------------------------
    //  拖动接口
    // ----------------------------------------------------------------

    /**
     * @brief 命中测试（鼠标按下时调用，判断是绘新 ROI 还是拖动已有 ROI）
     * @param screenX  屏幕像素 X
     * @param screenY  屏幕像素 Y
     * @return 命中结果；roiId == -1 表示未命中
     */
    FreehandHitResult HitTest(int screenX, int screenY) const;

    /**
     * @brief 平移指定 ROI（拖动中每帧调用）
     * @param roiId   目标 ROI ID
     * @param delta   世界坐标位移向量（法线方向分量应为 0）
     * @param viewer  用于重算统计
     */
    void MoveROI(int roiId,
        const std::array<double, 3>& delta,
        vtkImageViewer2* viewer);

    // ----------------------------------------------------------------
    //  删除
    // ----------------------------------------------------------------
    void DeleteLastFreehand();
    void ClearAllFreehand();

    // ----------------------------------------------------------------
    //  查询
    // ----------------------------------------------------------------
    FreehandStats GetLastStats() const;
    bool          IsDrawing()   const { return m_isDrawing; }

private:
    // ----------------------------------------------------------------
    //  内部数据结构
    // ----------------------------------------------------------------

    struct FreehandRecord {
        int      id = -1;
        ViewType viewType = ViewType::None;
        int      slice = 0;

        /// 轮廓点（世界坐标，按绘制顺序）
        std::vector<std::array<double, 3>> points;

        FreehandStats stats;
        bool          isComplete = false;

        // VTK 图元
        vtkSmartPointer<vtkActor>  outlineActor;  ///< 闭合轮廓线
        vtkSmartPointer<vtkActor>  fillActor;     ///< 半透明填充

        // 多行统计标签
        RoiLabel label;
    };

    // ----------------------------------------------------------------
    //  轴选择（与 SimpleROIManager 保持统一）
    // ----------------------------------------------------------------

    /**
     * @brief 根据视图方向获取平面轴和法线轴索引
     *
     *   Axial    → ax0=X(0)  ax1=Y(1)  axF=Z(2)
     *   Sagittal → ax0=Y(1)  ax1=Z(2)  axF=X(0)
     *   Coronal  → ax0=X(0)  ax1=Z(2)  axF=Y(1)
     */
    static void GetPlaneAxes(ViewType vt, int& ax0, int& ax1, int& axF);

    // ----------------------------------------------------------------
    //  VTK 图元构建（含视图平面修正）
    // ----------------------------------------------------------------

    /**
     * @brief 创建轮廓线 Actor（闭合折线，点在正确视图平面内）
     *
     * 修正：不再假设点在 XY 平面，直接使用世界坐标，
     * vtkPolyLine 在三维空间中连接各点，三视图均可见。
     */
    vtkSmartPointer<vtkActor> CreateOutlineActor(
        const std::vector<std::array<double, 3>>& pts);

    /**
     * @brief 创建填充面 Actor（vtkPolygon，半透明）
     *
     * 修正：同上，使用世界坐标，无平面假设。
     * 法线方向由视图决定（填充面朝向相机自然可见）。
     */
    vtkSmartPointer<vtkActor> CreateFillActor(
        const std::vector<std::array<double, 3>>& pts);

    // ----------------------------------------------------------------
    //  预览折线（绘制中实时更新）
    // ----------------------------------------------------------------
    void UpdatePreviewOutline(const std::vector<std::array<double, 3>>& pts);

    vtkSmartPointer<vtkPolyData>       m_previewPolyData;
    vtkSmartPointer<vtkPolyDataMapper> m_previewMapper;
    vtkSmartPointer<vtkActor>          m_previewActor;
    bool                               m_previewInitialized = false;

    // ----------------------------------------------------------------
    //  统计 / 标签
    // ----------------------------------------------------------------

    std::array<double, 3> ComputeLabelAnchor(
        const std::vector<std::array<double, 3>>& pts,
        ViewType viewType) const;

    static std::vector<std::string> BuildLabelLines(const FreehandStats& stats);

    FreehandStats ComputeStats(
        vtkImageViewer2* viewer,
        const std::vector<std::array<double, 3>>& pts,
        ViewType viewType, int slice) const;

    static bool IsPointInPolygon(
        double px, double py,
        const std::vector<std::array<double, 3>>& poly,
        int ax0, int ax1);

    static double ComputePerimeter(const std::vector<std::array<double, 3>>& pts);

    static double ComputeArea(
        const std::vector<std::array<double, 3>>& pts,
        int ax0, int ax1, double sp0, double sp1);

    // ----------------------------------------------------------------
    //  命中测试辅助
    // ----------------------------------------------------------------

    /**
     * @brief 世界坐标 → 屏幕坐标（用于命中测试）
     */
    bool WorldToScreen(const std::array<double, 3>& world,
        double& outX, double& outY) const;

    /**
     * @brief 判断屏幕点是否在多边形屏幕投影的包围盒内
     *        （AABB 命中测试，性能好，适合手绘轮廓）
     */
    bool IsScreenPointInROIBounds(int screenX, int screenY,
        const FreehandRecord& rec) const;

    // ----------------------------------------------------------------
    //  图元生命周期
    // ----------------------------------------------------------------
    void RebuildActors(FreehandRecord& rec);
    void RemoveFreehandActors(FreehandRecord& rec);

    int NextId() { return ++m_nextId; }

    // ----------------------------------------------------------------
    //  成员变量
    // ----------------------------------------------------------------
    vtkRenderer* m_overlayRenderer = nullptr;
    bool         m_initialized = false;
    bool         m_visible = true;

    std::unordered_map<int, FreehandRecord> m_records;
    int m_nextId = 0;
    int m_lastId = -1;

    bool                              m_isDrawing = false;
    ViewType                          m_drawView = ViewType::None;
    int                               m_drawSlice = 0;
    std::vector<std::array<double, 3>> m_drawPoints;

    // 命中测试容差（屏幕像素）
    static constexpr double kHitTolerancePx = 8.0;
    // 采样稀疏阈值（mm）
    static constexpr double kMinSampleDistMm = 1.5;
    // 标签偏移（mm）
    static constexpr double kLabelOffsetMm = 8.0;
    static constexpr double kLabelScale = 6.5;
    static constexpr double kLineSpacingMm = 9.0;
};
