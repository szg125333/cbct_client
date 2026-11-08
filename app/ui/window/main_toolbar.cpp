#include "main_toolbar.h"
#include "ui_main_toolbar.h"

#include <QDebug>
#include <QToolButton>

MainToolbar::MainToolbar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainToolbar)
{
    ui->setupUi(this);

    // objectName -> 中文功能名 / 图标资源路径
    //
    // 分组说明（与 .ui 中分隔符一致，按功能就近排布）：
    // 1 文件          打开 / 导出
    // 2 视图与布局    轴状位、多格、1x1~3x3 布局、隐藏边框
    // 3 3D            进入 3D、旋转/平移/缩放、3D 预设、混合比例
    // 4 显示调整      窗宽窗位预设、窗宽窗位、平移、缩放、边缘增强、像素探测、轮廓开关
    // 5 测量          直线距离、角度、多边形选取、绘制勾画、清除标注
    // 6 导航          十字准星、切片滑动
    // 7 图像编辑      裁剪/擦除/魔棒/画笔（引擎无算子，已从 .ui 隐藏 visible=false）
    // 8 剪贴板        新增/复制/粘贴（语义未定义，已隐藏）
    //   恢复方式：把 main_toolbar.ui 中对应按钮的 visible 改回 true（含 sep6/sep7）
    m_objectToAction = {
        {QStringLiteral("btnOpen"),        QStringLiteral("打开 DICOM")},
        {QStringLiteral("btnSave"),        QStringLiteral("保存")},

        {QStringLiteral("btnAxialView"),   QStringLiteral("轴状位视图")},
        {QStringLiteral("btn3DView"),      QStringLiteral("3D视图")},
        {QStringLiteral("btnMultiGrid"),   QStringLiteral("多格视图")},
        {QStringLiteral("btnLayout1x1"),   QStringLiteral("1x1布局")},
        {QStringLiteral("btnLayout1x2"),   QStringLiteral("1x2布局")},
        {QStringLiteral("btnLayout2x1"),   QStringLiteral("2x1布局")},
        {QStringLiteral("btnLayout2x2"),   QStringLiteral("2x2布局")},
        {QStringLiteral("btnLayout3x3"),   QStringLiteral("3x3布局")},
        {QStringLiteral("btnCustomGrid"),  QStringLiteral("自定义网格")},
        {QStringLiteral("btnHideBorder"),  QStringLiteral("隐藏边框")},

        {QStringLiteral("btnWwPreset"),    QStringLiteral("窗宽窗位预设")},
        {QStringLiteral("btnWindowLevel"), QStringLiteral("2D窗宽窗位")},
        {QStringLiteral("btnPan"),         QStringLiteral("平移")},
        {QStringLiteral("btnZoom"),        QStringLiteral("缩放")},
        {QStringLiteral("btnEdgeEnhance"), QStringLiteral("边缘增强")},
        {QStringLiteral("btnMeasureRuler"),QStringLiteral("测量标尺")}, // 测量：直线距离
        {QStringLiteral("btnPixelProbe"),  QStringLiteral("像素探测")}, // 测量：点像素值
        {QStringLiteral("btnContourToggle"),QStringLiteral("轮廓开关")},

        {QStringLiteral("btn3DRotate"),    QStringLiteral("3D旋转")},
        {QStringLiteral("btn3DPan"),       QStringLiteral("3D平移")},
        {QStringLiteral("btn3DZoom"),      QStringLiteral("3D缩放")},
        {QStringLiteral("btn3DPreset"),    QStringLiteral("3D预设")},
        {QStringLiteral("btnBlendRatio"),  QStringLiteral("混合比例")},
        {QStringLiteral("btnRectCrop"),    QStringLiteral("矩形裁剪")},
        {QStringLiteral("btnLassoCrop"),   QStringLiteral("套索裁剪")},
        {QStringLiteral("btnEraseMask"),   QStringLiteral("擦除掩膜")},

        {QStringLiteral("btnDrawContour"), QStringLiteral("绘制勾画")},
        {QStringLiteral("btnClearAll"),    QStringLiteral("清除标注")},
        {QStringLiteral("btnCutTrim"),     QStringLiteral("切割修剪")},
        {QStringLiteral("btnAngleMeasure"),QStringLiteral("角度测量")}, // 测量：三点角
        {QStringLiteral("btnPolygonSelect"),QStringLiteral("多边形选取")},
        {QStringLiteral("btnMagicThreshold"),QStringLiteral("魔棒阈值")},
        {QStringLiteral("btnMoveCrosshair"),QStringLiteral("移动准星")},
        {QStringLiteral("btnSliceScroll"), QStringLiteral("切片滑动")}, // 左键上下拖动换切片
        {QStringLiteral("btnBrush"),       QStringLiteral("画笔")},
        {QStringLiteral("btnEraser"),      QStringLiteral("橡皮擦")},

        {QStringLiteral("btnAdd"),         QStringLiteral("新增")},
        {QStringLiteral("btnCopy"),        QStringLiteral("复制")},
        {QStringLiteral("btnPaste"),       QStringLiteral("粘贴")},
    };

    // objectName -> 资源内 PNG 路径
    m_objectToIconPath = {
        {QStringLiteral("btnOpen"),        QStringLiteral(":/icons/open.png")},
        {QStringLiteral("btnSave"),        QStringLiteral(":/icons/save.png")},

        {QStringLiteral("btnAxialView"),   QStringLiteral(":/icons/axial_view.png")},
        {QStringLiteral("btn3DView"),      QStringLiteral(":/icons/3d_view.png")},
        {QStringLiteral("btnMultiGrid"),   QStringLiteral(":/icons/multi_grid.png")},
        {QStringLiteral("btnLayout1x1"),   QStringLiteral(":/icons/layout_1x1.png")},
        {QStringLiteral("btnLayout1x2"),   QStringLiteral(":/icons/layout_1x2.png")},
        {QStringLiteral("btnLayout2x1"),   QStringLiteral(":/icons/layout_2x1.png")},
        {QStringLiteral("btnLayout2x2"),   QStringLiteral(":/icons/layout_2x2.png")},
        {QStringLiteral("btnLayout3x3"),   QStringLiteral(":/icons/layout_3x3.png")},
        {QStringLiteral("btnCustomGrid"),  QStringLiteral(":/icons/custom_grid.png")},
        {QStringLiteral("btnHideBorder"),  QStringLiteral(":/icons/hide_border.png")},

        {QStringLiteral("btnWwPreset"),    QStringLiteral(":/icons/window_level.png")},
        {QStringLiteral("btnWindowLevel"), QStringLiteral(":/icons/window_level.png")},
        {QStringLiteral("btnPan"),         QStringLiteral(":/icons/pan.png")},
        {QStringLiteral("btnZoom"),        QStringLiteral(":/icons/zoom.png")},
        {QStringLiteral("btnEdgeEnhance"), QStringLiteral(":/icons/edge_enhance.png")},
        {QStringLiteral("btnMeasureRuler"),QStringLiteral(":/icons/measure_ruler.png")},
        {QStringLiteral("btnPixelProbe"),  QStringLiteral(":/icons/pixel_probe.png")},
        {QStringLiteral("btnContourToggle"),QStringLiteral(":/icons/contour_toggle.png")},

        {QStringLiteral("btn3DRotate"),    QStringLiteral(":/icons/3d_rotate.png")},
        {QStringLiteral("btn3DPan"),       QStringLiteral(":/icons/3d_pan.png")},
        {QStringLiteral("btn3DZoom"),      QStringLiteral(":/icons/3d_zoom.png")},
        {QStringLiteral("btn3DPreset"),    QStringLiteral(":/icons/preset_3d.png")},
        {QStringLiteral("btnBlendRatio"),  QStringLiteral(":/icons/blend_ratio.png")},
        {QStringLiteral("btnRectCrop"),    QStringLiteral(":/icons/rect_crop.png")},
        {QStringLiteral("btnLassoCrop"),   QStringLiteral(":/icons/lasso_crop.png")},
        {QStringLiteral("btnEraseMask"),   QStringLiteral(":/icons/erase_mask.png")},

        {QStringLiteral("btnDrawContour"), QStringLiteral(":/icons/draw_contour.png")},
        {QStringLiteral("btnClearAll"),    QStringLiteral(":/icons/clear_all.png")},
        {QStringLiteral("btnCutTrim"),     QStringLiteral(":/icons/cut_trim.png")},
        {QStringLiteral("btnAngleMeasure"),QStringLiteral(":/icons/angle_measure.png")},
        {QStringLiteral("btnPolygonSelect"),QStringLiteral(":/icons/polygon_select.png")},
        {QStringLiteral("btnMagicThreshold"),QStringLiteral(":/icons/magic_threshold.png")},
        {QStringLiteral("btnMoveCrosshair"),QStringLiteral(":/icons/move_crosshair.png")},
        {QStringLiteral("btnSliceScroll"), QStringLiteral(":/icons/slice_scroll.png")},
        {QStringLiteral("btnBrush"),       QStringLiteral(":/icons/brush.png")},
        {QStringLiteral("btnEraser"),      QStringLiteral(":/icons/eraser.png")},

        {QStringLiteral("btnAdd"),         QStringLiteral(":/icons/add.png")},
        {QStringLiteral("btnCopy"),        QStringLiteral(":/icons/copy.png")},
        {QStringLiteral("btnPaste"),       QStringLiteral(":/icons/paste.png")},
    };

    setupConnections();
    applyIconsAndTooltips();
}

MainToolbar::~MainToolbar()
{
    delete ui;
}

void MainToolbar::setButtonIcon(const QString &actionName, const QIcon &icon)
{
    QString objectName = actionName;
    if (!m_objectToAction.contains(actionName)) {
        for (auto it = m_objectToAction.constBegin(); it != m_objectToAction.constEnd(); ++it) {
            if (it.value() == actionName) {
                objectName = it.key();
                break;
            }
        }
    }

    auto *btn = findChild<QToolButton *>(objectName);
    if (btn) {
        btn->setIcon(icon);
    } else {
        qWarning() << "MainToolbar: no tool button for" << actionName;
    }
}

void MainToolbar::onToolButtonClicked()
{
    auto *btn = qobject_cast<QToolButton *>(sender());
    if (!btn) {
        return;
    }

    const QString actionName = actionNameOf(btn);
    qInfo().noquote() << QStringLiteral("[MainToolbar] %1").arg(actionName);
    emit actionTriggered(actionName);
    emit buttonClicked(btn->objectName(), btn->isChecked());
}

void MainToolbar::setupConnections()
{
    const auto buttons = findChildren<QToolButton *>();
    for (QToolButton *btn : buttons) {
        if (!m_objectToAction.contains(btn->objectName())) {
            continue;
        }
        // 纯图标按钮，文字只做悬停提示
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setAutoRaise(true);
        btn->setFixedSize(28, 28);
        btn->setIconSize(QSize(20, 20));
        btn->setText(QString());
        connect(btn, &QToolButton::clicked, this, &MainToolbar::onToolButtonClicked);
    }
}

void MainToolbar::applyIconsAndTooltips()
{
    for (auto it = m_objectToAction.constBegin(); it != m_objectToAction.constEnd(); ++it) {
        auto *btn = findChild<QToolButton *>(it.key());
        if (!btn) {
            continue;
        }

        btn->setToolTip(it.value());

        const QString iconPath = m_objectToIconPath.value(it.key());
        const QIcon icon(iconPath);
        if (icon.isNull()) {
            qWarning() << "MainToolbar: missing icon resource" << iconPath;
        }
        btn->setIcon(icon);
    }
}

QString MainToolbar::actionNameOf(QWidget *button) const
{
    return m_objectToAction.value(button->objectName(), button->objectName());
}
