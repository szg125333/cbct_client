/**
 * @file three_view_panel.h
 * @brief 右侧医学图像三视图：左半轴状位 + 右半上下 矢状/冠状
 *
 * 布局沿用 .ui 的 3 个 QVTKOpenGLNativeWidget；渲染/交互引擎为
 * DicomImageStudy 移植来的 VtkViewRenderer + ThreeViewController
 * （见 ui/view/engine/）。对外只暴露 setDicomVolume / setInteractionMode /
 * resetViews / clearMeasurements，VTK 细节不外泄。
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
class VtkViewRenderer;
class ThreeViewController;
class IViewRenderer;
struct RTStructureData;

namespace Ui {
class ThreeViewPanel;
}

/**
 * @class ThreeViewPanel
 * @brief 布局：viewAxial | viewSagittal / viewCoronal
 */
class ThreeViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeViewPanel(QWidget *parent = nullptr);
    ~ThreeViewPanel() override;

    /// 注入 DICOM 体数据（调用方/桥接层拥有 image 生命周期，内部 Register）
    void setDicomVolume(vtkImageData *image);

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

    ThreeViewController *controller() const { return m_controller; }

    void renderAll(); ///< 三个RenderWindow 刷新

private:
    void setupEngine();

    Ui::ThreeViewPanel *ui;

    std::unique_ptr<VtkViewRenderer> m_renderers[3];
    ThreeViewController *m_controller = nullptr; ///< 由本面板创建并拥有

    vtkImageData *m_image = nullptr; ///< 当前体数据（引用计数 +1）
    ImageBlend m_blend;              ///< CT/CBCT 混合控制器
};

#endif // THREE_VIEW_PANEL_H
