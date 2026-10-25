#include "main_toolbar.h"
#include "ui_main_toolbar.h"

#include <QDebug>
#include <QToolButton>

MainToolbar::MainToolbar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainToolbar)
{
    ui->setupUi(this);

    // objectName -> 悬停提示 / 点击打印用的功能名
    //
    // 分组说明（与 .ui 中分隔符一致）：
    // 1 文件   2 视图/布局  3 2D 测量与窗宽窗位  4 3D 操作
    // 5 勾画/角度/选取等测量编辑  6 剪贴板
    //
    // 测量相关（首版仅 UI 占位，逻辑后续挂到视图交互）：
    //   btnMeasureRuler  直线距离标尺（mm，需像素间距）
    //   btnAngleMeasure  角度测量（三点角）
    //   btnPixelProbe    单像素 CT 值/HU 探测
    //   btnMoveCrosshair 十字线定位
    m_objectToAction = {
        {QStringLiteral("btnOpen"),        QStringLiteral("打开文件")},
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

        {QStringLiteral("btnWindowLevel"), QStringLiteral("2D窗宽窗位")},
        {QStringLiteral("btnPan"),         QStringLiteral("平移")},
        {QStringLiteral("btnZoom"),        QStringLiteral("缩放")},
        {QStringLiteral("btnEdgeEnhance"), QStringLiteral("边缘增强")},
        {QStringLiteral("btnMeasureRuler"),QStringLiteral("测量标尺")}, // 测量：直线距离
        {QStringLiteral("btnPixelProbe"),  QStringLiteral("像素探测")}, // 测量：点像素值

        {QStringLiteral("btn3DRotate"),    QStringLiteral("3D旋转")},
        {QStringLiteral("btn3DPan"),       QStringLiteral("3D平移")},
        {QStringLiteral("btn3DZoom"),      QStringLiteral("3D缩放")},
        {QStringLiteral("btnRectCrop"),    QStringLiteral("矩形裁剪")},
        {QStringLiteral("btnLassoCrop"),   QStringLiteral("套索裁剪")},
        {QStringLiteral("btnEraseMask"),   QStringLiteral("擦除掩膜")},

        {QStringLiteral("btnDrawContour"), QStringLiteral("绘制勾画")},
        {QStringLiteral("btnCutTrim"),     QStringLiteral("切割修剪")},
        {QStringLiteral("btnAngleMeasure"),QStringLiteral("角度测量")}, // 测量：三点角
        {QStringLiteral("btnPolygonSelect"),QStringLiteral("多边形选取")},
        {QStringLiteral("btnMagicThreshold"),QStringLiteral("魔棒阈值")},
        {QStringLiteral("btnMoveCrosshair"),QStringLiteral("移动准星")},
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

        {QStringLiteral("btnWindowLevel"), QStringLiteral(":/icons/window_level.png")},
        {QStringLiteral("btnPan"),         QStringLiteral(":/icons/pan.png")},
        {QStringLiteral("btnZoom"),        QStringLiteral(":/icons/zoom.png")},
        {QStringLiteral("btnEdgeEnhance"), QStringLiteral(":/icons/edge_enhance.png")},
        {QStringLiteral("btnMeasureRuler"),QStringLiteral(":/icons/measure_ruler.png")},
        {QStringLiteral("btnPixelProbe"),  QStringLiteral(":/icons/pixel_probe.png")},

        {QStringLiteral("btn3DRotate"),    QStringLiteral(":/icons/3d_rotate.png")},
        {QStringLiteral("btn3DPan"),       QStringLiteral(":/icons/3d_pan.png")},
        {QStringLiteral("btn3DZoom"),      QStringLiteral(":/icons/3d_zoom.png")},
        {QStringLiteral("btnRectCrop"),    QStringLiteral(":/icons/rect_crop.png")},
        {QStringLiteral("btnLassoCrop"),   QStringLiteral(":/icons/lasso_crop.png")},
        {QStringLiteral("btnEraseMask"),   QStringLiteral(":/icons/erase_mask.png")},

        {QStringLiteral("btnDrawContour"), QStringLiteral(":/icons/draw_contour.png")},
        {QStringLiteral("btnCutTrim"),     QStringLiteral(":/icons/cut_trim.png")},
        {QStringLiteral("btnAngleMeasure"),QStringLiteral(":/icons/angle_measure.png")},
        {QStringLiteral("btnPolygonSelect"),QStringLiteral(":/icons/polygon_select.png")},
        {QStringLiteral("btnMagicThreshold"),QStringLiteral(":/icons/magic_threshold.png")},
        {QStringLiteral("btnMoveCrosshair"),QStringLiteral(":/icons/move_crosshair.png")},
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
