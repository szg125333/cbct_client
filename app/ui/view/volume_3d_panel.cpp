#include "volume_3d_panel.h"

#include <QVTKOpenGLNativeWidget.h>
#include <QVBoxLayout>

#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkImageResample.h>
#include <vtkMatrix3x3.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkRenderWindow.h>
#include <vtkVolumeProperty.h>

#include <QShowEvent>
#include <QTimer>

#include <algorithm>

#include "common/log_service.h"

namespace {

/**
 * @brief 3D 显示专用副本：降采样 + 去方向矩阵
 *
 * 1. vtkImageResample 把最大边缩到 maxDim（默认 256），软件光线投射才跟得上；
 * 2. VTK9 体绘制不支持非单位方向矩阵（会画空/错位），这里强制
 *    单位方向 + 原点归零——仅影响 3D 显示，不改 2D 三视图数据。
 */
vtkImageData *prepareVolumeForDisplay(vtkImageData *src, int maxDim = 256)
{
    if (!src) {
        CBCT_LOG_WARN("ui", "3D prep: src=null");
        return nullptr;
    }

    int dims[3];
    src->GetDimensions(dims);
    const int maxd = std::max({dims[0], dims[1], dims[2]});
    const double factor = (maxd > maxDim) ? (static_cast<double>(maxDim) / maxd) : 1.0;

    CBCT_LOG_DEBUG("ui", "3D prep: src {}x{}x{} type={} factor={:.3f}",
                  dims[0], dims[1], dims[2],
                  src->GetScalarTypeAsString(), factor);

    vtkNew<vtkImageResample> res;
    res->SetInputData(src);
    if (factor < 1.0) {
        res->SetAxisMagnificationFactor(0, factor);
        res->SetAxisMagnificationFactor(1, factor);
        res->SetAxisMagnificationFactor(2, factor);
    }
    res->Update();

    auto *out = vtkImageData::New();
    out->DeepCopy(res->GetOutput());

    // vtkFixedPointVolumeRayCastMapper 只吃 unsigned char / unsigned short，
    // signed short 会静默黑屏。本数据标量非负（[0,3583]），值不变直接转即可，
    // 不做缩放——转移动函数仍按原始像素标定。
    if (out->GetScalarType() != VTK_UNSIGNED_SHORT &&
        out->GetScalarType() != VTK_UNSIGNED_CHAR) {
        double sr[2];
        out->GetScalarRange(sr);
        CBCT_LOG_WARN("ui", "3D prep: 标量类型 {} 不受支持，转 unsigned short (range=[{:.0f},{:.0f}])",
                      out->GetScalarTypeAsString(), sr[0], sr[1]);

        vtkNew<vtkImageData> converted;
        converted->SetDimensions(out->GetDimensions());
        converted->SetSpacing(out->GetSpacing());
        converted->SetOrigin(out->GetOrigin());
        converted->AllocateScalars(VTK_UNSIGNED_SHORT, 1);

        const vtkIdType count = static_cast<vtkIdType>(out->GetNumberOfPoints());
        const double lo = (sr[0] < 0.0) ? sr[0] : 0.0;  // 仅在有负值时整体平移
        const short *srcPtr = static_cast<const short *>(out->GetScalarPointer());
        unsigned short *dstPtr = static_cast<unsigned short *>(converted->GetScalarPointer());
        for (vtkIdType i = 0; i < count; ++i) {
            double v = static_cast<double>(srcPtr[i]) - lo;
            if (v < 0.0) v = 0.0;
            if (v > 65535.0) v = 65535.0;
            dstPtr[i] = static_cast<unsigned short>(v + 0.5);
        }
        converted->GetPointData()->GetScalars()->Modified();
        converted->Modified();

        out->DeepCopy(converted);
    }

    // 单位方向 + 原点归零（体绘制不认方向矩阵）
    vtkNew<vtkMatrix3x3> identity;
    identity->Identity();
    out->SetDirectionMatrix(identity);
    out->SetOrigin(0.0, 0.0, 0.0);
    out->Modified();

    int od[3];
    out->GetDimensions(od);
    double osp[3];
    out->GetSpacing(osp);
    double osr[2];
    out->GetScalarRange(osr);
    CBCT_LOG_DEBUG("ui", "3D prep: out {}x{}x{} type={} spacing=({:.3f},{:.3f},{:.3f}) scalar=[{:.0f},{:.0f}]",
                  od[0], od[1], od[2], out->GetScalarTypeAsString(),
                  osp[0], osp[1], osp[2], osr[0], osr[1]);
    return out;
}

}  // namespace

vtkStandardNewMacro(ModeStyle);

Volume3dPanel::Volume3dPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_widget = new QVTKOpenGLNativeWidget(this);
    m_widget->setFocusPolicy(Qt::StrongFocus);
    m_widget->setContextMenuPolicy(Qt::NoContextMenu);
    layout->addWidget(m_widget);

    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_widget->setRenderWindow(m_renderWindow);

    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(0.08, 0.08, 0.12);  // 深色背景（医学影像常规）
    m_renderWindow->AddRenderer(m_renderer);

    m_mapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    // vmwgfx 上软件光线投射（vtkFixedPoint）静默失败 → 黑屏；
    // 已开 VMware 3D 加速，改走 GPU 光线投射（OpenGL2）
    m_mapper->SetBlendModeToComposite();
    m_volume = vtkSmartPointer<vtkVolume>::New();
    m_volume->SetMapper(m_mapper);
    m_renderer->AddViewProp(m_volume);

    m_color = vtkSmartPointer<vtkColorTransferFunction>::New();
    m_opacity = vtkSmartPointer<vtkPiecewiseFunction>::New();

    if (auto *iren = m_renderWindow->GetInteractor()) {
        m_style = vtkSmartPointer<ModeStyle>::New();
        iren->SetInteractorStyle(m_style);
    }

    // 初始预设（未加载体数据时也给出正确外观）
    setPreset(VolumePreset::SoftTissue);
}

Volume3dPanel::~Volume3dPanel() = default;

void Volume3dPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // QStackedWidget 切页后布局/上下文未就绪，Render 会静默失败；
    // 推迟到事件循环，等 show 完成、尺寸确定后再画
    scheduleRender();
}

void Volume3dPanel::scheduleRender()
{
    // QVTKOpenGLNativeWidget 必须走 Qt 重绘（paintEvent 内部才调 Render），
    // 直接调 renderWindow->Render() 不会刷到屏幕 → 黑屏
    QTimer::singleShot(0, this, [this]() {
        if (!m_widget) return;
        CBCT_LOG_DEBUG("ui", "3D render {}x{} visible={}",
                       m_widget->width(), m_widget->height(), m_widget->isVisible());
        m_widget->update();
    });
}

void Volume3dPanel::setVolume(vtkImageData *image, double slope, double intercept)
{
    if (!image) return;
    m_slope = (slope == 0.0) ? 1.0 : slope;
    m_intercept = intercept;

    // 3D 显示专用：降采样到 256³ 内 + 去方向矩阵（VTK 体绘制不支持方向矩阵）
    m_displayImage = prepareVolumeForDisplay(image);
    if (!m_displayImage) return;

    int d[3];
    m_displayImage->GetDimensions(d);
    CBCT_LOG_INFO("ui", "3D 体绘制 {}x{}x{} -> {}x{}x{} slope={} intercept={}",
                  image->GetDimensions()[0], image->GetDimensions()[1],
                  image->GetDimensions()[2], d[0], d[1], d[2],
                  m_slope, m_intercept);

    m_mapper->SetInputData(m_displayImage);
    m_mapper->Update();
    applyTransferFunctions();
    resetView();
}

void Volume3dPanel::setPreset(VolumePreset preset)
{
    m_preset = preset;
    applyTransferFunctions();
}

void Volume3dPanel::setInteractionMode(View3dMode mode)
{
    m_mode = mode;
    if (m_style) m_style->SetMode(mode);
}

void Volume3dPanel::resetView()
{
    if (!m_renderer) return;

    // 确定性相机：直接用体包围盒算，不依赖 ResetCamera 的隐式推断
    // （体绘制 mapper 异常时 ResetCamera 会算出离谱的相机 → 只见背景）
    double b[6] = {0, 0, 0, 0, 0, 0};
    const double *vb = m_volume ? m_volume->GetBounds() : nullptr;
    if (vb && vb[1] > vb[0]) {
        for (int i = 0; i < 6; ++i) b[i] = vb[i];
    } else {
        b[0] = b[2] = b[4] = 0.0;
        b[1] = b[3] = b[5] = 498.0;  // 退化时用默认体尺寸
    }

    const double cx = (b[0] + b[1]) / 2.0;
    const double cy = (b[2] + b[3]) / 2.0;
    const double cz = (b[4] + b[5]) / 2.0;
    const double dx = b[1] - b[0];
    const double dy = b[3] - b[2];
    const double dz = b[5] - b[4];
    const double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double dist = diag * 1.6;  // 斜 45° 视角能装下整个体

    if (auto *cam = m_renderer->GetActiveCamera()) {
        cam->SetFocalPoint(cx, cy, cz);
        // 从右上前方看
        cam->SetPosition(cx + dist * 0.55, cy - dist * 0.55, cz + dist * 0.62);
        cam->SetViewUp(0, 0, 1);
        // 裁剪范围必须覆盖整个场景，否则全部被裁掉（只剩背景色）
        cam->SetClippingRange(diag * 0.01, dist * 3.0);
    }
    m_renderer->ResetCameraClippingRange();
    scheduleRender();

    if (auto *cam = m_renderer->GetActiveCamera()) {
        double clip[2];
        cam->GetClippingRange(clip);
        CBCT_LOG_DEBUG("ui", "3D 相机复位 dist={:.1f} clip=[{:.1f},{:.1f}] 体对角线={:.1f}",
                       cam->GetDistance(), clip[0], clip[1], diag);
    }
}

void Volume3dPanel::applyTransferFunctions()
{
    // 预设按 HU 定义；体数据是原始像素，换算 raw = (HU - intercept) / slope
    auto toRaw = [this](double hu) { return (hu - m_intercept) / m_slope; };

    m_color->RemoveAllPoints();
    m_opacity->RemoveAllPoints();

    // 兜底：实际标量范围（若 slope/intercept 约定不同导致预设范围不重叠，
    // 用实际范围重建渐变，保证无论如何有图像可见）
    double sr[2] = {0.0, 1.0};
    bool haveRange = false;
    if (m_mapper && m_mapper->GetInput()) {
        m_mapper->GetInput()->GetScalarRange(sr);
        haveRange = (sr[1] > sr[0]);
    }

    double huLo = (m_preset == VolumePreset::Bone) ? -200.0 : -200.0;
    double huHi = (m_preset == VolumePreset::Bone) ? 2000.0 : 400.0;
    double rawLo = toRaw(huLo);
    double rawHi = toRaw(huHi);

    if (haveRange && (rawHi < sr[0] || rawLo > sr[1])) {
        // 完全不重叠 → 直接按实际范围做渐变
        CBCT_LOG_WARN("ui", "3D 预设范围 [{:.0f},{:.0f}] 与标量 [{:.0f},{:.0f}] 不重叠，改用实际范围",
                      rawLo, rawHi, sr[0], sr[1]);
        rawLo = sr[0];
        rawHi = sr[1];
    }

    // 以 rawLo/rawHi 为锚点构造 5 个控制点（覆盖 [rawLo, rawHi]）
    auto at = [rawLo, rawHi](double t) { return rawLo + t * (rawHi - rawLo); };

    if (m_preset == VolumePreset::Bone) {
        // 骨窗：软组织几乎透明，骨白色渐显
        m_color->AddRGBPoint(at(0.0), 0.35, 0.30, 0.25);
        m_color->AddRGBPoint(at(0.25), 0.85, 0.78, 0.70);
        m_color->AddRGBPoint(at(0.6), 1.0, 0.98, 0.95);
        m_color->AddRGBPoint(at(1.0), 1.0, 1.0, 1.0);

        m_opacity->AddPoint(at(0.0), 0.0);
        m_opacity->AddPoint(at(0.2), 0.0);
        m_opacity->AddPoint(at(0.35), 0.15);
        m_opacity->AddPoint(at(0.65), 0.55);
        m_opacity->AddPoint(at(1.0), 0.85);
    } else {
        // 软组织窗：中低段渐显
        m_color->AddRGBPoint(at(0.0), 0.25, 0.15, 0.15);
        m_color->AddRGBPoint(at(0.3), 0.75, 0.45, 0.40);
        m_color->AddRGBPoint(at(0.6), 0.95, 0.80, 0.72);
        m_color->AddRGBPoint(at(1.0), 1.0, 0.98, 0.95);

        m_opacity->AddPoint(at(0.0), 0.0);
        m_opacity->AddPoint(at(0.2), 0.05);
        m_opacity->AddPoint(at(0.5), 0.25);
        m_opacity->AddPoint(at(0.8), 0.55);
        m_opacity->AddPoint(at(1.0), 0.80);
    }

    if (auto *prop = m_volume->GetProperty()) {
        prop->SetColor(m_color);
        prop->SetScalarOpacity(m_opacity);
        // 关闭梯度不透明度：未显式设置梯度 TF 时部分驱动会静默全黑
        prop->SetDisableGradientOpacity(1);
        prop->SetInterpolationTypeToLinear();
        // 光照：让体绘制有立体感（配合 ShadeOff 若驱动不支持可改回）
        prop->ShadeOn();
        prop->SetAmbient(0.30);
        prop->SetDiffuse(0.70);
        prop->SetSpecular(0.20);
        prop->SetSpecularPower(10.0);
    }

    scheduleRender();
}
