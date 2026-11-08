#include "renderer/OverlayManager/SimpleOverlayManager.h"
#include "renderer/OverlayManager/OverlayInfoManager/SimpleOverlayInfoManager.h"
#include "ContourOverlayManager/SimpleContourOverlayManager.h"

#include <vtkRenderer.h>
#include <vtkImageViewer2.h>

// ============================================================
//  构造 / 析构
// ============================================================

SimpleOverlayManager::SimpleOverlayManager() = default;
SimpleOverlayManager::~SimpleOverlayManager() { Shutdown(); }

// ============================================================
//  生命周期
// ============================================================

void SimpleOverlayManager::Initialize(vtkRenderer* overlayRenderer,
    vtkImageViewer2* viewer)
{
    if (m_initialized || !overlayRenderer) return;

    m_overlayRenderer = overlayRenderer;
    m_viewer = viewer;

    // 依次初始化所有已注册的 Feature
    for (auto& feature : m_features) {
        if (feature) {
            feature->Initialize(m_overlayRenderer);
        }
    }

    SetVisible(m_visible);
    SetColor(m_color[0], m_color[1], m_color[2]);

    m_initialized = true;
}

void SimpleOverlayManager::Shutdown()
{
    if (!m_initialized) return;

    m_viewer = nullptr;
    m_overlayRenderer = nullptr;
    m_initialized = false;
}

// ============================================================
//  Feature 注册
// ============================================================

void SimpleOverlayManager::RegisterFeature(std::unique_ptr<IOverlayFeature> feature)
{
    if (!feature) return;

    // 若已初始化，立即初始化新注册的 Feature
    if (m_initialized && m_overlayRenderer) {
        feature->Initialize(m_overlayRenderer);
    }
    m_features.push_back(std::move(feature));
}

IOverlayFeature* SimpleOverlayManager::GetFeatureImpl(const std::type_info& type)
{
    for (auto& feature : m_features) {
        if (feature && typeid(*feature) == type) {
            return feature.get();
        }
    }
    return nullptr;
}

// ============================================================
//  样式控制
// ============================================================

void SimpleOverlayManager::SetVisible(bool visible)
{
    m_visible = visible;
    // 各 Feature 内部持有 Actor，由 Feature 自行处理可见性
}

void SimpleOverlayManager::SetColor(double r, double g, double b)
{
    m_color[0] = r;
    m_color[1] = g;
    m_color[2] = b;
}

// ============================================================
//  图像边界
// ============================================================

void SimpleOverlayManager::SetImageWorldBounds(const std::array<double, 6>& bounds)
{
    m_imageWorldBounds = bounds;
    m_hasImageBounds = true;
}

bool SimpleOverlayManager::IsWorldPointInImage(
    const std::array<double, 3>& worldPoint) const
{
    // 未设置边界时保守允许（不会误拒合法操作）
    if (!m_hasImageBounds) return true;

    constexpr double kEps = 1e-3;  // 容差，避免浮点边界误判
    const auto& b = m_imageWorldBounds;
    const auto& p = worldPoint;

    return (p[0] >= b[0] - kEps && p[0] <= b[1] + kEps &&
        p[1] >= b[2] - kEps && p[1] <= b[3] + kEps &&
        p[2] >= b[4] - kEps && p[2] <= b[5] + kEps);
}

// ============================================================
//  驱动接口
// ============================================================

bool SimpleOverlayManager::Update(const EventData& /*event*/)
{
    // 预留扩展接口，暂不使用
    return false;
}

bool SimpleOverlayManager::OnSliceChanged(ViewType viewType, int slice)
{
    for (auto& feature : m_features) {
        if (feature) {
            feature->OnSliceChanged(m_viewer, slice, viewType);
        }
    }
    return true;
}

void SimpleOverlayManager::UpdateBasicInfoActor(const RenderViewState& state)
{
    auto* infoFeature = GetFeature<SimpleOverlayInfoManager>();
    if (infoFeature) {
        infoFeature->SetVisible(true);
        infoFeature->Update(state);
    }
}

void SimpleOverlayManager::SetRTStructureData(std::shared_ptr<RTStructureData> data) {
    auto* feature = GetFeature<SimpleContourOverlayManager>();
    if (feature) {
        feature->SetRTStructureData(data);
    }
}


std::vector<ROIDisplayInfo> SimpleOverlayManager::GetROIList() const {
    // 从 SimpleContourOverlayManager 获取（需要给它也加这个方法）
    auto* feature = const_cast<SimpleOverlayManager*>(this)
        ->GetFeature<SimpleContourOverlayManager>();
    if (feature) return feature->GetROIList();
    return {};
}

void SimpleOverlayManager::SetROIVisible(int roiNumber, bool visible) {
    auto* feature = GetFeature<SimpleContourOverlayManager>();
    if (feature) feature->SetROIVisible(roiNumber, visible);
}