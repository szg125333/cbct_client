#include "three_view_panel.h"
#include "ui_three_view_panel.h"

#include "controller/ThreeViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/VtkViewRenderer.h"

#include <QVTKOpenGLNativeWidget.h>

#include <vtkImageData.h>

ThreeViewPanel::ThreeViewPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ThreeViewPanel)
{
    ui->setupUi(this);
    setupEngine();
}

ThreeViewPanel::~ThreeViewPanel()
{
    // 不做 VTK 深度释放：VMware/GLEW 下 render window 析构可能卡死
    // （与 MainWindow 摘面板不 delete 的 hack 配套）；进程退出由 OS 回收。
    m_controller = nullptr;  // 面板拥有，随 Qt 树析构

    delete ui;
}

void ThreeViewPanel::setupEngine()
{
    QVTKOpenGLNativeWidget *widgets[3] = {
        ui->viewAxial, ui->viewSagittal, ui->viewCoronal};
    const ViewType orientations[3] = {
        ViewType::Axial, ViewType::Sagittal, ViewType::Coronal};

    for (int i = 0; i < 3; ++i) {
        // Escape/键盘需要焦点；禁用右键菜单，避免抢走 VTK 的 RightButtonPress
        widgets[i]->setFocusPolicy(Qt::StrongFocus);
        widgets[i]->setContextMenuPolicy(Qt::NoContextMenu);
        m_renderers[i] = std::make_unique<VtkViewRenderer>(widgets[i]);
        m_renderers[i]->SetOrientation(orientations[i]);
    }

    m_controller = new ThreeViewController(this);
    std::array<IViewRenderer *, 3> arr = {
        m_renderers[0].get(), m_renderers[1].get(), m_renderers[2].get()};
    m_controller->SetRenderers(arr);
}

void ThreeViewPanel::setDicomVolume(vtkImageData *image)
{
    if (m_image == image) {
        return;
    }
    if (m_image) {
        m_image->UnRegister(nullptr);
    }
    m_image = image;
    if (m_image) {
        m_image->Register(nullptr);
    }

    // 混合控制器基准图 = 主图（对齐原版 SetImageData → SetPrimaryImage）
    m_blend.SetPrimaryImage(m_image);

    if (m_controller) {
        m_controller->SetImageData(m_blend.GetBlendedImage());
        m_controller->ResetAllViews();
    }
    renderAll();
}

void ThreeViewPanel::setSecondaryImage(vtkImageData *image)
{
    // CBCT 等叠加图（须已重采样到与主图同网格，否则 ImageBlend 拒绝）
    m_blend.SetSecondaryImage(image);
    if (m_controller) {
        m_controller->SetImageData(m_blend.GetBlendedImage());
    }
    renderAll();
}

void ThreeViewPanel::setBlendRatio(double ratio)
{
    m_blend.SetBlendRatio(ratio);
    if (m_controller) {
        m_controller->SetImageData(m_blend.GetBlendedImage());
    }
    renderAll();
}

double ThreeViewPanel::blendRatio() const
{
    return m_blend.GetBlendRatio();
}

void ThreeViewPanel::setInteractionMode(InteractionMode mode)
{
    if (m_controller) {
        m_controller->SetInteractionMode(mode);
    }
}

void ThreeViewPanel::setRTStructureData(std::shared_ptr<RTStructureData> data)
{
    if (m_controller) {
        m_controller->SetRTStructureData(std::move(data));
    }
    renderAll();
}

std::vector<ROIDisplayInfo> ThreeViewPanel::roiList() const
{
    if (!m_controller) return {};
    auto *r = m_controller->GetRenderer(0);
    if (!r) return {};
    auto *om = r->GetOverlayManager();
    return om ? om->GetROIList() : std::vector<ROIDisplayInfo>{};
}

void ThreeViewPanel::setRoiVisible(int roiNumber, bool visible)
{
    if (!m_controller) return;
    for (int i = 0; i < 3; ++i) {
        auto *r = m_controller->GetRenderer(i);
        if (!r) continue;
        auto *om = r->GetOverlayManager();
        if (!om) continue;
        om->SetROIVisible(roiNumber, visible);
        // SetROIVisible 只改标志，须重建轮廓 actor（对齐原版 SetROIVisible）
        om->OnSliceChanged(static_cast<ViewType>(i), r->GetSlice());
        r->RequestRender();
    }
}

std::vector<MeasurementRecord> ThreeViewPanel::measurementRecords() const
{
    return m_controller ? m_controller->CollectMeasurementRecords()
                        : std::vector<MeasurementRecord>{};
}

void ThreeViewPanel::setModeToMeasurement(InteractionMode mode, bool state)
{
    if (!m_controller) return;

    // 严格对齐原版 ThreeViewWidget::setModeToMeasurement：
    //   取消选中 → 切到该模式清其绘制，再回 Normal（所有标注消失）
    //   选中     → 先回 Normal 清干净，再进入目标模式（干净起步）
    if (!state) {
        m_controller->SetInteractionMode(mode);
        m_controller->ClearAllStrategyDrawings();
        m_controller->SetInteractionMode(InteractionMode::Normal);
        renderAll();
        return;
    }
    m_controller->SetInteractionMode(InteractionMode::Normal);
    m_controller->ClearAllStrategyDrawings();
    m_controller->SetInteractionMode(mode);
    renderAll();
}

void ThreeViewPanel::resetViews()
{
    if (m_controller) {
        m_controller->ResetAllViews();
    }
    renderAll();
}

void ThreeViewPanel::clearMeasurements()
{
    if (m_controller) {
        m_controller->ClearAllStrategyDrawings();
    }
    renderAll();
}

void ThreeViewPanel::renderAll()
{
    for (int i = 0; i < 3; ++i) {
        if (m_renderers[i]) {
            m_renderers[i]->RequestRender();
        }
    }
}
