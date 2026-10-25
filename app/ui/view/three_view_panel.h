/**
 * @file three_view_panel.h
 * @brief 右侧医学图像三视图：左半轴状位 + 右半上下 矢状/冠状
 *
 * 显示控件为 VTK 的 QVTKOpenGLNativeWidget（需与系统 VTK/Qt 版本匹配）。
 * 当前主要为 3D/重建预留；采集 2D 在旁边的 AcquisitionImagePanel。
 */
#ifndef THREE_VIEW_PANEL_H
#define THREE_VIEW_PANEL_H

#include <QWidget>

class vtkImageData;           ///< VTK 体数据/图像数据指针（不拥有细节在此头）
class QVTKOpenGLNativeWidget; ///< VTK-Qt 渲染 widget
class vtkRenderer;            ///< 每个视图一个 renderer

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

    /// 三视图共用一份 vtkImageData（调用方负责生命周期；内部 Register）
    void setVtkImageData(vtkImageData *image);

    void renderAll(); ///< 三个RenderWindow 刷新

private:
    void setupView(QVTKOpenGLNativeWidget *widget, vtkRenderer *&renderer);
    void applyWindowLevel(double window, double level); ///< 窗宽窗位（默认 CT 风格）

    Ui::ThreeViewPanel *ui;

    vtkRenderer *m_axialRenderer;    ///< 左：轴状位
    vtkRenderer *m_sagittalRenderer; ///< 右上：矢状位
    vtkRenderer *m_coronalRenderer;  ///< 右下：冠状位

    vtkImageData *m_image; ///< 当前体/图像数据（引用计数 +1）
};

#endif // THREE_VIEW_PANEL_H
