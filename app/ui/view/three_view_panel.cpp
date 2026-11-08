#include "three_view_panel.h"
#include "ui_three_view_panel.h"

#include "controller/ThreeViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/VtkViewRenderer.h"

#include <QGridLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkImageData.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageMathematics.h>
#include <vtkNew.h>

namespace {

/// 非锐化掩膜：out = src + amount * (src - blur(src))，几何不变
vtkImageData *buildUnsharpMask(vtkImageData *src)
{
    if (!src) return nullptr;

    vtkNew<vtkImageGaussianSmooth> blur;
    blur->SetInputData(src);
    blur->SetStandardDeviation(1.0);
    blur->Update();

    vtkNew<vtkImageMathematics> sub;  // high = src - blur
    sub->SetOperationToSubtract();
    sub->SetInputData(0, src);
    sub->SetInputData(1, blur->GetOutput());
    sub->Update();

    vtkNew<vtkImageMathematics> mul;  // scaled = high * 1.0
    mul->SetOperationToMultiplyByK();
    mul->SetConstantK(1.0);
    mul->SetInputData(0, sub->GetOutput());
    mul->Update();

    vtkNew<vtkImageMathematics> add;  // out = src + scaled
    add->SetOperationToAdd();
    add->SetInputData(0, src);
    add->SetInputData(1, mul->GetOutput());
    add->Update();

    auto *out = vtkImageData::New();
    out->DeepCopy(add->GetOutput());
    return out;
}

}  // namespace

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
    if (m_enhancedImage) {
        m_enhancedImage->UnRegister(nullptr);
        m_enhancedImage = nullptr;
    }

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

    // 布局网格：摆放三个视图（布局切换用）
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(4, 4, 4, 4);
    m_grid->setSpacing(4);
    applyViewLayout();

    m_controller = new ThreeViewController(this);
    std::array<IViewRenderer *, 3> arr = {
        m_renderers[0].get(), m_renderers[1].get(), m_renderers[2].get()};
    m_controller->SetRenderers(arr);
}

void ThreeViewPanel::applyViewLayout()
{
    auto *axial = ui->viewAxial;
    auto *sag = ui->viewSagittal;
    auto *cor = ui->viewCoronal;

    // 先从网格摘掉（不 delete widget）
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        delete item;
    }

    auto place = [this](QWidget *w, int row, int col, int rowSpan, int colSpan) {
        w->show();
        m_grid->addWidget(w, row, col, rowSpan, colSpan);
    };
    auto hideAll = [&]() {
        ui->viewAxial->hide();
        ui->viewSagittal->hide();
        ui->viewCoronal->hide();
    };

    hideAll();
    switch (m_layout) {
    case ViewLayout::OneByOne:
        place(axial, 0, 0, 1, 1);
        break;
    case ViewLayout::OneByTwo:
        place(axial, 0, 0, 1, 1);
        place(sag, 0, 1, 1, 1);
        break;
    case ViewLayout::TwoByOne:
        place(axial, 0, 0, 1, 1);
        place(cor, 1, 0, 1, 1);
        break;
    case ViewLayout::TwoByTwo:
        place(axial, 0, 0, 2, 1);
        place(sag, 0, 1, 1, 1);
        place(cor, 1, 1, 1, 1);
        break;
    case ViewLayout::ThreeByOne:
        place(axial, 0, 0, 1, 1);
        place(sag, 0, 1, 1, 1);
        place(cor, 0, 2, 1, 1);
        break;
    }
    renderAll();
}

void ThreeViewPanel::setViewLayout(ViewLayout layout)
{
    if (m_layout == layout) return;
    m_layout = layout;
    applyViewLayout();
}

void ThreeViewPanel::setEdgeEnhanced(bool on)
{
    if (m_edgeEnhanced == on) return;
    m_edgeEnhanced = on;

    if (m_edgeEnhanced && !m_enhancedImage) {
        m_enhancedImage = buildUnsharpMask(m_blend.GetBlendedImage());
    }

    // 显示图来源：开增强用增强图，否则用混合结果（几何一致，测量坐标不受影响）
    if (m_controller) {
        vtkImageData *show = (m_edgeEnhanced && m_enhancedImage)
                                 ? m_enhancedImage
                                 : m_blend.GetBlendedImage();
        m_controller->SetImageData(show);
        renderAll();
    }
}

void ThreeViewPanel::setBordersHidden(bool hidden)
{
    m_bordersHidden = hidden;
    // 视图控件边框
    const QString style = hidden ? QString() : QStringLiteral("border: 1px solid #888;");
    ui->viewAxial->setStyleSheet(style);
    ui->viewSagittal->setStyleSheet(style);
    ui->viewCoronal->setStyleSheet(style);
    // 视图间不再有分隔框（布局网格自适应），仅调间距
    m_grid->setSpacing(hidden ? 0 : 4);
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

    // 重建边缘增强缓存（几何随新图）
    if (m_enhancedImage) {
        m_enhancedImage->UnRegister(nullptr);
        m_enhancedImage = nullptr;
    }
    if (m_edgeEnhanced && m_blend.GetBlendedImage()) {
        m_enhancedImage = buildUnsharpMask(m_blend.GetBlendedImage());
    }

    if (m_controller) {
        vtkImageData *show = (m_edgeEnhanced && m_enhancedImage)
                                 ? m_enhancedImage
                                 : m_blend.GetBlendedImage();
        m_controller->SetImageData(show);
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
