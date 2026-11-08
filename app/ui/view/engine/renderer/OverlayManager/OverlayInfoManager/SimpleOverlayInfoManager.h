#pragma once

#include "IOverlayInfoManager.h"
#include <vtkSmartPointer.h>
#include <vtkTextActor.h>

class vtkRenderer;

class SimpleOverlayInfoManager : public IOverlayInfoManager {
public:
    explicit SimpleOverlayInfoManager();
    ~SimpleOverlayInfoManager() override;

    void Initialize(vtkRenderer* overlayRenderer) override;
    void Shutdown() override;
    void Update(const RenderViewState& state) override;
    void SetVisible(bool visible) override;
    void SetColor(double r, double g, double b) override;
    void OnSliceChanged(vtkImageViewer2* viewer,int slice,ViewType viewType) override;

    // 可选：设置要显示的字段（默认全显示）
    void SetEnabledFields(const std::vector<OverlayField>& fields);
    void SetCustomFormat(const std::string& format); // 如 "{view} | WW:{ww} WL:{wl}"
    //void SetImageWorldBounds(const std::array<double, 6>& bounds)override;

private:
    void buildDisplayText(const RenderViewState& state);

private:
    vtkSmartPointer<vtkRenderer> m_overlayRenderer;
    vtkSmartPointer<vtkTextActor> m_textActor;
    bool m_initialized = false;
    bool m_visible = true;
    std::vector<OverlayField> m_enabledFields;
    std::string m_customFormat;
};