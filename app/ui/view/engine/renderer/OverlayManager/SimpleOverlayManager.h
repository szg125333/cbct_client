#pragma once

#include "IOverlayManager.h"
#include <vector>
#include <memory>

/**
 * @brief Overlay 管理器的默认实现
 *
 * 以 vector 持有所有 IOverlayFeature，统一完成初始化、切片同步和信息刷新。
 * 通过 OverlayFactory::CreateDefault() 创建并注入标准功能集。
 */
class SimpleOverlayManager : public IOverlayManager {
public:
    SimpleOverlayManager();
    ~SimpleOverlayManager() override;

    // IOverlayManager 接口
    void Initialize(vtkRenderer* overlayRenderer, vtkImageViewer2* viewer)   override;
    void Shutdown()                                                          override;
    void SetVisible(bool visible)                                            override;
    void SetColor(double r, double g, double b)                              override;
    void SetImageWorldBounds(const std::array<double, 6>& bounds)            override;
    bool IsWorldPointInImage(const std::array<double, 3>& worldPoint) const  override;
    bool Update(const EventData& event)                                      override;
    bool OnSliceChanged(ViewType viewType, int slice)                        override;
    void UpdateBasicInfoActor(const RenderViewState& state)                  override;
    void SetRTStructureData(std::shared_ptr<RTStructureData> data)           override;
    std::vector<ROIDisplayInfo> GetROIList() const                           override;
    void SetROIVisible(int roiNumber, bool visible)                          override;
    /**
     * @brief 注册一个 Overlay Feature
     *
     * 若管理器已初始化，则立即对新 Feature 调用 Initialize。
     */
    void RegisterFeature(std::unique_ptr<IOverlayFeature> feature);

protected:
    IOverlayFeature* GetFeatureImpl(const std::type_info& type) override;

private:
    vtkRenderer* m_overlayRenderer = nullptr;
    vtkImageViewer2* m_viewer = nullptr;

    std::vector<std::unique_ptr<IOverlayFeature>> m_features;

    bool   m_initialized = false;
    bool   m_visible = true;
    double m_color[3] = { 1.0, 1.0, 0.0 };   // 默认黄色

    bool                  m_hasImageBounds = false;
    std::array<double, 6> m_imageWorldBounds = {};
};
