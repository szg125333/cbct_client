#ifndef THREE_VIEW_PANEL_H
#define THREE_VIEW_PANEL_H

#include <QWidget>

class vtkImageData;
class QVTKOpenGLNativeWidget;
class vtkRenderer;

namespace Ui {
class ThreeViewPanel;
}

/// 右侧医学图像三视图面板：轴状位 / 矢状位 / 冠状位
/// 使用 QVTKOpenGLNativeWidget 显示 vtkImageData
class ThreeViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeViewPanel(QWidget *parent = nullptr);
    ~ThreeViewPanel() override;

    /// 设置三视图共用的体数据（或同一 vtkImageData 的切片来源）
    void setVtkImageData(vtkImageData *image);

    /// 刷新渲染
    void renderAll();

private:
    void setupView(QVTKOpenGLNativeWidget *widget, vtkRenderer *&renderer);
    void applyWindowLevel(double window, double level);

    Ui::ThreeViewPanel *ui;

    vtkRenderer *m_axialRenderer;
    vtkRenderer *m_sagittalRenderer;
    vtkRenderer *m_coronalRenderer;

    vtkImageData *m_image;
};

#endif // THREE_VIEW_PANEL_H
