#include "three_view_panel.h"
#include "ui_three_view_panel.h"

#include <QVTKOpenGLNativeWidget.h>

#include <vtkImageData.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageProperty.h>
#include <vtkInteractorStyleImage.h>
#include <vtkProp.h>
#include <vtkPropCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

ThreeViewPanel::ThreeViewPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ThreeViewPanel)
    , m_axialRenderer(nullptr)
    , m_sagittalRenderer(nullptr)
    , m_coronalRenderer(nullptr)
    , m_image(nullptr)
{
    ui->setupUi(this);

    setupView(ui->viewAxial, m_axialRenderer);
    setupView(ui->viewSagittal, m_sagittalRenderer);
    setupView(ui->viewCoronal, m_coronalRenderer);
}

ThreeViewPanel::~ThreeViewPanel()
{
    if (m_image) {
        m_image->Delete();
        m_image = nullptr;
    }

    // renderer 由 widget 的 RenderWindow 持有，这里只释放引用
    if (m_axialRenderer) {
        m_axialRenderer->Delete();
        m_axialRenderer = nullptr;
    }
    if (m_sagittalRenderer) {
        m_sagittalRenderer->Delete();
        m_sagittalRenderer = nullptr;
    }
    if (m_coronalRenderer) {
        m_coronalRenderer->Delete();
        m_coronalRenderer = nullptr;
    }

    delete ui;
}

void ThreeViewPanel::setVtkImageData(vtkImageData *image)
{
    if (m_image == image) {
        return;
    }

    if (m_image) {
        m_image->Delete();
    }
    m_image = image;
    if (m_image) {
        m_image->Register(nullptr);
    }

    vtkRenderer *renderers[] = {m_axialRenderer, m_sagittalRenderer, m_coronalRenderer};
    for (vtkRenderer *ren : renderers) {
        if (!ren) {
            continue;
        }
        ren->RemoveAllViewProps();
        if (!m_image) {
            continue;
        }

        auto *actor = vtkImageActor::New();
        actor->GetMapper()->SetInputData(m_image);
        ren->AddActor(actor);
        actor->Delete();

        ren->ResetCamera();
    }

    // 默认窗宽窗位（灰度 CT 常用范围，可按实际数据调整）
    applyWindowLevel(2000.0, 400.0);
    renderAll();
}

void ThreeViewPanel::renderAll()
{
    vtkRenderWindow *windows[] = {
        ui->viewAxial->renderWindow(),
        ui->viewSagittal->renderWindow(),
        ui->viewCoronal->renderWindow(),
    };
    for (vtkRenderWindow *rw : windows) {
        if (rw) {
            rw->Render();
        }
    }
}

void ThreeViewPanel::setupView(QVTKOpenGLNativeWidget *widget, vtkRenderer *&renderer)
{
    auto *ren = vtkRenderer::New();
    ren->SetBackground(0.12, 0.12, 0.14);
    widget->setRenderWindow(widget->renderWindow());
    widget->renderWindow()->AddRenderer(ren);
    widget->renderWindow()->SetMultiSamples(0);

    auto *style = vtkInteractorStyleImage::New();
    widget->interactor()->SetInteractorStyle(style);
    style->Delete();

    // 保存引用（renderWindow 持有 ren，我们再 Register 一次便于释放）
    ren->Register(nullptr);
    renderer = ren;
    ren->Delete();
}

void ThreeViewPanel::applyWindowLevel(double window, double level)
{
    vtkRenderer *renderers[] = {m_axialRenderer, m_sagittalRenderer, m_coronalRenderer};
    for (vtkRenderer *ren : renderers) {
        if (!ren) {
            continue;
        }
        // 遍历 actors 设置窗宽窗位
        vtkCollectionSimpleIterator it;
        // 简化：通过 camera 后再统一处理；有 image actor 时用 color window
        (void)window;
        (void)level;
        (void)it;
    }
    // 有数据时设置 color window/level
    if (!m_image) {
        return;
    }
    vtkRenderer *renderers2[] = {m_axialRenderer, m_sagittalRenderer, m_coronalRenderer};
    for (vtkRenderer *ren : renderers2) {
        if (!ren) {
            continue;
        }
        vtkPropCollection *props = ren->GetViewProps();
        props->InitTraversal();
        while (vtkProp *prop = props->GetNextProp()) {
            if (auto *actor = vtkImageActor::SafeDownCast(prop)) {
                actor->GetProperty()->SetColorWindow(window);
                actor->GetProperty()->SetColorLevel(level);
            }
        }
    }
}
