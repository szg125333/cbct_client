/**
 * @file volume_3d_panel.h
 * @brief 3D 体绘制面板（vtkSmartVolumeMapper）+ 骨/软组织预设
 *
 * 新功能（原版 DicomImageStudy 无 3D，工具栏 3D 按钮原为占位）。
 * 显示 bridge 传入的 vtkImageData，转移动函数按 HU 定义；
 * 交互：btn3DRotate/btn3DPan/btn3DZoom 切换左键拖拽行为。
 */
#ifndef VOLUME_3D_PANEL_H
#define VOLUME_3D_PANEL_H

#include <QWidget>

#include <vtkColorTransferFunction.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkVolume.h>
#include <vtkGPUVolumeRayCastMapper.h>

class QVTKOpenGLNativeWidget;

/// 3D 交互模式（左键拖拽行为）
enum class View3dMode {
    Rotate,
    Pan,
    Zoom,
};

/// 3D 显示预设（按 HU 定义）
enum class VolumePreset {
    Bone,       ///< 骨窗：200~2000 HU
    SoftTissue, ///< 软组织窗：-100~200 HU
};

/**
 * @brief 左键行为按 mode 分派：Rotate / Pan / Zoom
 *
 * vtkInteractorStyleTrackballCamera 的 StartRotate/StartPan/StartDolly 是 protected，
 * 子类里可以直接调用，从而把三种操作绑到左键上。
 */
class ModeStyle : public vtkInteractorStyleTrackballCamera {
public:
    static ModeStyle *New();
    vtkTypeMacro(ModeStyle, vtkInteractorStyleTrackballCamera);

    void SetMode(View3dMode mode) { m_mode = mode; }

    void OnLeftButtonDown() override {
        switch (m_mode) {
        case View3dMode::Rotate: StartRotate(); break;
        case View3dMode::Pan:    StartPan();    break;
        case View3dMode::Zoom:   StartDolly();  break;
        }
    }
    void OnLeftButtonUp() override {
        switch (m_mode) {
        case View3dMode::Rotate: EndRotate(); break;
        case View3dMode::Pan:    EndPan();    break;
        case View3dMode::Zoom:   EndDolly();  break;
        }
    }

private:
    View3dMode m_mode = View3dMode::Rotate;
};

class Volume3dPanel : public QWidget
{
    Q_OBJECT

public:
    explicit Volume3dPanel(QWidget *parent = nullptr);
    ~Volume3dPanel() override;

    /// 设置体数据（原始像素；slope/intercept 用于按 HU 定义预设）
    void setVolume(vtkImageData *image, double slope, double intercept);

    void setPreset(VolumePreset preset);
    void setInteractionMode(View3dMode mode);
    /// 相机复位（正面偏角）
    void resetView();

protected:
    /// 面板显示时延迟重绘（QStackedWidget 切页 + 布局完成后再画）
    void showEvent(QShowEvent *event) override;

private:
    void applyTransferFunctions();
    /// 推迟到事件循环里渲染（隐藏状态下 Render 会静默失败）
    void scheduleRender();

    QVTKOpenGLNativeWidget *m_widget = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_mapper;
    vtkSmartPointer<vtkVolume> m_volume;
    vtkSmartPointer<vtkColorTransferFunction> m_color;
    vtkSmartPointer<vtkPiecewiseFunction> m_opacity;
    vtkSmartPointer<ModeStyle> m_style;

    /// 3D 显示专用副本（降采样 + 单位方向），持有引用计数
    vtkSmartPointer<vtkImageData> m_displayImage;

    double m_slope = 1.0;
    double m_intercept = 0.0;
    VolumePreset m_preset = VolumePreset::SoftTissue;
    View3dMode m_mode = View3dMode::Rotate;
};

#endif  // VOLUME_3D_PANEL_H
