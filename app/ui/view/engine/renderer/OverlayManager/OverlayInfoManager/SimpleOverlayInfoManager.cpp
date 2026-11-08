#include "SimpleOverlayInfoManager.h"
#include <vtkRenderer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <QStringList>
#include <QString>
#include <sstream>
#include <iomanip>

SimpleOverlayInfoManager::SimpleOverlayInfoManager() {
    // 默认显示所有字段
    m_enabledFields = {
        OverlayField::ViewType,
        OverlayField::WindowWidth,
        OverlayField::WindowLevel,
        OverlayField::SliceIndex,
        OverlayField::WorldPosition
    };
}

SimpleOverlayInfoManager::~SimpleOverlayInfoManager() {
    Shutdown();
}

void SimpleOverlayInfoManager::Initialize(vtkRenderer* overlayRenderer) {
    if (m_initialized || !overlayRenderer) return;

    m_overlayRenderer = overlayRenderer;
    m_textActor = vtkSmartPointer<vtkTextActor>::New();
    m_textActor->GetTextProperty()->SetColor(1.0, 1.0, 0.0); // 黄色
    m_textActor->GetTextProperty()->SetFontSize(18);
    m_textActor->SetDisplayPosition(10, 10); // 左下角
    m_overlayRenderer->AddActor2D(m_textActor);

    m_initialized = true;
    m_visible = true;
}

void SimpleOverlayInfoManager::Shutdown() {
    if (!m_initialized) return;
    if (m_overlayRenderer && m_textActor) {
        m_overlayRenderer->RemoveActor2D(m_textActor);
    }
    m_textActor = nullptr;
    m_overlayRenderer = nullptr;
    m_initialized = false;
}

void SimpleOverlayInfoManager::SetVisible(bool visible) {
    m_visible = visible;
    if (m_initialized) {
        m_textActor->SetVisibility(visible ? 1 : 0);
    }
}

void SimpleOverlayInfoManager::SetEnabledFields(const std::vector<OverlayField>& fields) {
    m_enabledFields = fields;
}

void SimpleOverlayInfoManager::SetCustomFormat(const std::string& format) {
    m_customFormat = format;
}

//void SimpleOverlayInfoManager::SetImageWorldBounds(const std::array<double, 6>& bounds)
//{
//}

void SimpleOverlayInfoManager::Update(const RenderViewState& state) {
    if (!m_initialized) return;

    buildDisplayText(state);
    m_textActor->SetVisibility(m_visible ? 1 : 0);
}

void SimpleOverlayInfoManager::SetColor(double r, double g, double b) {

}

void SimpleOverlayInfoManager::OnSliceChanged(vtkImageViewer2* viewer,int slice,ViewType viewType)
{
}

void SimpleOverlayInfoManager::buildDisplayText(const RenderViewState& state) {
    QString text;

    // 如果有自定义格式，优先使用（可后续扩展）
    if (!m_customFormat.empty()) {
        // 简单占位符替换（可增强为模板引擎）
        QString fmt = QString::fromStdString(m_customFormat);
        fmt.replace("{view}",
            state.viewType == ViewType::Axial ? "Axial" :
            state.viewType == ViewType::Sagittal ? "Sagittal" :
            state.viewType == ViewType::Coronal ? "Coronal" : "Unknown");
        fmt.replace("{ww}", QString::number(state.windowWidth, 'f', 0));
        fmt.replace("{wl}", QString::number(state.windowLevel, 'f', 0));
        fmt.replace("{slice}", QString::number(state.sliceIndex));
        fmt.replace("{x}", QString::number(state.worldPos[0], 'f', 2));
        fmt.replace("{y}", QString::number(state.worldPos[1], 'f', 2));
        fmt.replace("{z}", QString::number(state.worldPos[2], 'f', 2));
        m_textActor->SetInput(fmt.toLocal8Bit().constData());
        return;
    }

    // 默认格式：逐行显示启用的字段
    QStringList lines;

    auto hasField = [&](OverlayField f) {
        return std::find(m_enabledFields.begin(), m_enabledFields.end(), f) != m_enabledFields.end();
        };

    if (hasField(OverlayField::ViewType)) {
        QString viewStr =
            state.viewType == ViewType::Axial ? "Axial" :
            state.viewType == ViewType::Sagittal ? "Sagittal" :
            state.viewType == ViewType::Coronal ? "Coronal" : "Unknown";
        lines << viewStr;
    }

    if (hasField(OverlayField::WindowWidth) || hasField(OverlayField::WindowLevel)) {
        lines << QString("WW: %1  WL: %2")
            .arg(state.windowWidth, 0, 'f', 0)
            .arg(state.windowLevel, 0, 'f', 0);
    }

    if (hasField(OverlayField::SliceIndex)) {
        lines << QString("Slice: %1").arg(state.sliceIndex);
    }

    if (hasField(OverlayField::WorldPosition)) {
        lines << QString("Pos: (%1, %2, %3)")
            .arg(state.worldPos[0], 0, 'f', 2)
            .arg(state.worldPos[1], 0, 'f', 2)
            .arg(state.worldPos[2], 0, 'f', 2);
    }

    // TODO: 支持 VoxelValue 和 CustomFields

    m_textActor->SetInput(lines.join("\n").toLocal8Bit().constData());
}