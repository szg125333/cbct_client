#pragma once

#include "renderer/OverlayManager/IOverlayFeature.h"
#include "RoiStats.h"
#include "common/Measurement/MeasurementTypes.h"
#include "RoiLabel.h"

#include <array>
#include <vector>
#include <unordered_map>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkImageViewer2;
class vtkActor;
class vtkCamera;
class vtkLineSource;
class vtkPolyDataMapper;
class vtkPoints;

// ----------------------------------------------------------------
//  拖动命中类型
// ----------------------------------------------------------------

enum class RoiHitType {
    None,
    Body,       ///< 边框或内部（整体平移）
    CornerTL,   ///< 左上角
    CornerTR,   ///< 右上角
    CornerBL,   ///< 左下角
    CornerBR,   ///< 右下角
};

struct RoiHitResult {
    int        roiId = -1;
    RoiHitType hitType = RoiHitType::None;
};

// ----------------------------------------------------------------
//  SimpleROIManager
// ----------------------------------------------------------------

/**
 * @brief ROI 矩形框 Overlay Feature
 *
 * 功能：
 *   - 左键拖拽绘制矩形框，释放后固定并计算统计
 *   - 统计标签以多行形式显示在矩形框正下方
 *   - 支持整体拖动和角点缩放
 *   - 切片切换后坐标自动投影，统计值重算
 *   - 三视图（Axial/Sagittal/Coronal）均正确显示
 *
 * 修正记录：
 *   - CreateCornerSquare 增加 ViewType 参数，根据视图平面确定
 *     方块的偏移方向，修复 Sagittal/Coronal 视图中角点不可见的问题
 */
class SimpleROIManager : public IOverlayFeature {
public:
    SimpleROIManager();
    ~SimpleROIManager() override;

    // ----------------------------------------------------------------
    //  IOverlayFeature 接口
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
    void BeginROI(const std::array<double, 3>& cornerWorld,
        ViewType viewType, int slice);
    void UpdatePreview(const std::array<double, 3>& oppositeCorner);
    void CommitROI(const std::array<double, 3>& oppositeCorner,
        vtkImageViewer2* viewer);
    void CancelCurrentROI();

    // ----------------------------------------------------------------
    //  拖动接口
    // ----------------------------------------------------------------
    RoiHitResult HitTest(int screenX, int screenY) const;
    void MoveROI(int roiId,
        const std::array<double, 3>& delta,
        vtkImageViewer2* viewer);
    void ResizeROI(int roiId,
        RoiHitType hitType,
        const std::array<double, 3>& newCornerWorld,
        vtkImageViewer2* viewer);

    // ----------------------------------------------------------------
    //  删除接口
    // ----------------------------------------------------------------
    void DeleteLastROI();
    void ClearAllROI();

    // ----------------------------------------------------------------
    //  查询接口
    // ----------------------------------------------------------------
    RoiStats GetLastStats() const;
    RoiStats GetStats(int roiId) const;
    bool     IsDrawing() const { return m_isDrawing; }

    /// 导出用：当前全部矩形 ROI 的统计（含未完成）
    std::vector<RoiStatsRecord> GetStatsRecords() const;

private:
    // ----------------------------------------------------------------
    //  内部数据结构
    // ----------------------------------------------------------------

    struct RoiRecord {
        int      id = -1;
        ViewType viewType = ViewType::None;
        int      slice = 0;

        std::array<double, 3> corner1 = { 0, 0, 0 };   ///< 轴对齐最小角
        std::array<double, 3> corner2 = { 0, 0, 0 };   ///< 轴对齐最大角

        RoiStats stats;
        bool     isComplete = false;

        // 边框：4 条 LineSource
        struct BorderLine {
            vtkSmartPointer<vtkLineSource>     source;
            vtkSmartPointer<vtkPolyDataMapper> mapper;
            vtkSmartPointer<vtkActor>          actor;
        };
        BorderLine borderLines[4];

        // 半透明填充
        vtkSmartPointer<vtkActor> fillActor;

        // 四个角点方块（用于拖动命中提示）
        vtkSmartPointer<vtkActor> cornerActors[4];

        // 多行统计标签
        RoiLabel label;
    };

    // ----------------------------------------------------------------
    //  私有方法
    // ----------------------------------------------------------------

    void ComputeRectCorners(const std::array<double, 3>& c1,
        const std::array<double, 3>& c2,
        ViewType viewType,
        std::array<double, 3> out[4]) const;

    std::array<double, 3> ComputeLabelAnchor(
        const std::array<double, 3> corners[4],
        ViewType viewType) const;

    static std::vector<std::string> BuildLabelLines(const RoiStats& stats);

    void InitBorderLines(RoiRecord& roi);
    void UpdateBorderLines(RoiRecord& roi,
        const std::array<double, 3> corners[4]);
    void InitFillActor(RoiRecord& roi,
        const std::array<double, 3> corners[4]);
    void UpdateFillActor(RoiRecord& roi,
        const std::array<double, 3> corners[4]);
    void InitCornerActors(RoiRecord& roi,
        const std::array<double, 3> corners[4]);
    void UpdateCornerActors(RoiRecord& roi,
        const std::array<double, 3> corners[4]);

    /**
     * @brief 创建角点小方块 Actor
     *
     * @param center    方块中心世界坐标
     * @param viewType  视图方向（决定偏移在哪两个轴上进行）
     * @param halfSize  方块半边长（默认 2.0 mm）
     *
     * 三视图平面轴对应：
     *   Axial    → X/Y 平面偏移，Z 固定
     *   Sagittal → Y/Z 平面偏移，X 固定
     *   Coronal  → X/Z 平面偏移，Y 固定
     */
    vtkSmartPointer<vtkActor> CreateCornerSquare(
        const std::array<double, 3>& center,
        ViewType viewType,
        double halfSize = 2.0);

    void RedrawROI(RoiRecord& roi,
        const std::array<double, 3> corners[4],
        const std::array<double, 3>& anchor);

    void RemoveRoiActors(RoiRecord& roi);

    RoiStats ComputeStats(vtkImageViewer2* viewer,
        const std::array<double, 3>& c1,
        const std::array<double, 3>& c2,
        ViewType viewType, int slice) const;

    bool WorldToScreen(const std::array<double, 3>& world,
        double& outX, double& outY) const;

    int NextId() { return ++m_nextId; }

    // ----------------------------------------------------------------
    //  预览线
    // ----------------------------------------------------------------
    struct PreviewLine {
        vtkSmartPointer<vtkLineSource>     source;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor>          actor;
    };
    PreviewLine m_previewLines[4];
    bool        m_previewInitialized = false;

    // ----------------------------------------------------------------
    //  成员变量
    // ----------------------------------------------------------------
    vtkRenderer* m_overlayRenderer = nullptr;
    bool         m_initialized = false;
    bool         m_visible = true;

    std::unordered_map<int, RoiRecord> m_rois;
    int m_nextId = 0;
    int m_lastId = -1;

    bool                  m_isDrawing = false;
    std::array<double, 3> m_drawCorner1 = { 0, 0, 0 };
    ViewType              m_drawView = ViewType::None;
    int                   m_drawSlice = 0;

    static constexpr double kCornerTolerancePx = 8.0;
    static constexpr double kEdgeTolerancePx = 5.0;
    static constexpr double kLabelOffsetMm = 6.0;
    static constexpr double kLabelScale = 6.5;
    static constexpr double kLineSpacingMm = 9.0;
};
