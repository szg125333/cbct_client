/**
 * @file three_view_panel.h
 * @brief 右侧医学图像三视图：布局可切换（1x1/1x2/2x1/2x2/3x3）
 *
 * 布局由 .ui 的 3 个 QVTKOpenGLNativeWidget + 代码里 QGridLayout 摆放；
 * 渲染/交互引擎为 DicomImageStudy 移植来的 VtkViewRenderer + ThreeViewController
 * （见 ui/view/engine/）。
 */
#ifndef THREE_VIEW_PANEL_H
#define THREE_VIEW_PANEL_H

#include <QWidget>
#include <array>
#include <memory>

#include "common/InteractionMode.h"
#include "common/ViewTypes.h"
#include "common/ROIDisplayInfo.h"
#include "common/Measurement/MeasurementTypes.h"
#include "controller/ImageBlend.h"

class vtkImageData;
class QVTKOpenGLNativeWidget;
class QGridLayout;
class VtkViewRenderer;
class ThreeViewController;
class IViewRenderer;
struct RTStructureData;

namespace Ui {
class ThreeViewPanel;
}

/// 窗宽窗位预设（按 HU 定义，内部换算到原始像素域）
enum class WwPreset {
    Bone,       ///< 骨窗 2000/400
    SoftTissue, ///< 软组织窗 400/40
    Lung,       ///< 肺窗 1500/-600
    Abdomen,    ///< 腹部窗 350/50
};

/// 视图布局（工具栏布局组）
enum class ViewLayout {
    OneByOne,   ///< 仅轴状位
    OneByTwo,   ///< 轴状 | 矢状
    TwoByOne,   ///< 轴状 / 冠状
    TwoByTwo,   ///< 轴状 | 矢状/冠状（默认）
    ThreeByOne, ///< 三视图一行
};

class ThreeViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeViewPanel(QWidget *parent = nullptr);
    ~ThreeViewPanel() override;

    /// 注入 DICOM 体数据（调用方/桥接层拥有 image 生命周期，内部 Register）
    /// slope/intercept 用于把 HU 定义的窗宽窗位预设换算到原始像素域
    void setDicomVolume(vtkImageData *image, double slope = 1.0, double intercept = 0.0);

    /// 交互模式（普通/测量/平移等），转发 ThreeViewController
    void setInteractionMode(InteractionMode mode);

    /// 注入 RTSTRUCT 轮廓（解析在 core/dicom，bridge 转换后传入）
    void setRTStructureData(std::shared_ptr<RTStructureData> data);

    /// 设置叠加图像（CBCT，须与主图同网格）并刷新混合显示
    void setSecondaryImage(vtkImageData *image);

    /// 混合比例 0=纯主图 1=纯叠加图（对齐原版 SetBlendRatio）
    void setBlendRatio(double ratio);
    double blendRatio() const;

    /// ROI 列表（轮廓列表弹窗用；未加载 RTSTRUCT 时为空）
    std::vector<ROIDisplayInfo> roiList() const;

    /// 导出用：汇总全部距离/角度测量
    std::vector<MeasurementRecord> measurementRecords() const;

    /// 导出用：汇总全部 ROI 统计（矩形 + 手绘）
    std::vector<RoiStatsRecord> roiStats() const;

    /// 应用窗宽窗位预设（HU→原始像素域换算后下发给 controller）
    void applyWwPreset(WwPreset preset);

    /// 单个 ROI 显隐
    void setRoiVisible(int roiNumber, bool visible);

    /**
     * @brief 测量模式按钮选中/取消选中（对齐原版 ThreeViewWidget::setModeToMeasurement）
     * @param state true=选中（清干净后进入 mode）；false=取消选中（清掉绘制后回 Normal）
     */
    void setModeToMeasurement(InteractionMode mode, bool state);

    /// 重置三视图（相机、窗宽窗位、居中切片、清测量）
    void resetViews();

    /// 清除全部测量/标注
    void clearMeasurements();

    /// 视图布局切换（工具栏布局组按钮）
    void setViewLayout(ViewLayout layout);
    ViewLayout viewLayout() const { return m_layout; }

    /// 边缘增强开/关（对当前显示图做非锐化掩膜，几何不变）
    void setEdgeEnhanced(bool on);
    bool edgeEnhanced() const { return m_edgeEnhanced; }

    /// 隐藏边框：去掉视图间分隔与控件边框
    void setBordersHidden(bool hidden);
    bool bordersHidden() const { return m_bordersHidden; }

    ThreeViewController *controller() const { return m_controller; }

    void renderAll(); ///< 三个RenderWindow 刷新

private:
    void setupEngine();
    void applyViewLayout();

    Ui::ThreeViewPanel *ui;

    std::unique_ptr<VtkViewRenderer> m_renderers[3];
    ThreeViewController *m_controller = nullptr; ///< 由本面板创建并拥有
    QGridLayout *m_grid = nullptr;

    vtkImageData *m_image = nullptr; ///< 当前体数据（引用计数 +1）
    ImageBlend m_blend;              ///< CT/CBCT 混合控制器
    double m_slope = 1.0;            ///< 像素→HU 系数（窗宽窗位预设换算用）
    double m_intercept = 0.0;

    ViewLayout m_layout = ViewLayout::TwoByTwo;
    bool m_edgeEnhanced = false;
    bool m_bordersHidden = false;
    vtkImageData *m_enhancedImage = nullptr; ///< 边缘增强结果（引用计数 +1）
};

#endif // THREE_VIEW_PANEL_H
