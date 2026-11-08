#include "ThreeViewController.h"
#include "interface/IViewRenderer.h"
#include "renderer/OverlayManager/IOverlayManager.h"
#include "renderer/OverlayManager/OverlayFactory.h"
#include "renderer/OverlayManager/DistanceMeasureManager/SimpleDistanceMeasureManager.h"
#include "renderer/OverlayManager/AngleMeasureManager/SimpleAngleMeasureManager.h"
#include "controller/Strategy/InteractionStrategyFactory.h"
#include "controller/Strategy/IInteractionStrategy.h"
#include "controller/Strategy/ImageDragStrategy/ImageDragStrategy.h"

#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkRenderWindow.h>
#include <vtkImageViewer2.h>

#include <cmath>
#include <algorithm>
#include <QDebug>

// ============================================================
//  构造 / 析构
// ============================================================

ThreeViewController::ThreeViewController(QObject* parent)
	: QObject(parent)
{
	m_renderers.fill(nullptr);
	m_strategies = InteractionStrategyFactory::CreateStrategies(this);
	m_currentMode = InteractionMode::Normal;
}

ThreeViewController::~ThreeViewController() = default;

// ============================================================
//  初始化
// ============================================================

void ThreeViewController::SetRenderers(std::array<IViewRenderer*, 3> renderers)
{
	m_renderers = renderers;
}

void ThreeViewController::SetImageData(vtkImageData* image)
{
	if (!image) return;
	m_image = image;

	for (auto* r : m_renderers) {
		if (r) r->SetInputData(image);
	}

	ComputeSliceRanges();

	// 初始切片：图像各方向中心
	int dims[3];
	m_image->GetDimensions(dims);

	m_initialSlice[static_cast<int>(ViewType::Axial)] = dims[2] / 2;
	m_initialSlice[static_cast<int>(ViewType::Sagittal)] = dims[0] / 2;
	m_initialSlice[static_cast<int>(ViewType::Coronal)] = dims[1] / 2;

	SetSliceInternal(ViewType::Axial, m_initialSlice[static_cast<int>(ViewType::Axial)]);
	SetSliceInternal(ViewType::Sagittal, m_initialSlice[static_cast<int>(ViewType::Sagittal)]);
	SetSliceInternal(ViewType::Coronal, m_initialSlice[static_cast<int>(ViewType::Coronal)]);

	// 初始窗宽窗位：根据图像灰度范围推算
	double range[2];
	m_image->GetScalarRange(range);
	m_initialWindowWidth = range[1] - range[0];
	m_initialWindowLevel = (range[0] + range[1]) / 2.0;

	m_windowWidth = m_initialWindowWidth;
	m_windowLevel = m_initialWindowLevel;

	for (auto* r : m_renderers) {
		if (r) {
			r->SetColorWindow(m_windowWidth);
			r->SetColorLevel(m_windowLevel);
		}
	}

	// 正交投影并复位相机
	for (auto* r : m_renderers) {
		if (!r) continue;
		auto viewer = r->GetViewer();
		if (viewer && viewer->GetRenderer()) {
			viewer->GetRenderer()->GetActiveCamera()->SetParallelProjection(true);
			viewer->GetRenderer()->ResetCamera();
		}
	}

	RegisterEventCallbacks();
}

// ============================================================
//  IViewController 接口
// ============================================================

IViewRenderer* ThreeViewController::GetRenderer(int viewIndex)
{
	if (viewIndex < 0 || viewIndex >= 3) return nullptr;
	return m_renderers[viewIndex];
}

const vtkImageData* ThreeViewController::GetImage() const
{
	return m_image.Get();
}

void ThreeViewController::ChangeSlice(int viewIndex, int delta)
{
	if (viewIndex < 0 || viewIndex >= 3) return;

	const ViewType view = static_cast<ViewType>(viewIndex);
	int newSlice = m_renderers[viewIndex]
		? m_renderers[viewIndex]->GetSlice() + delta
		: 0;

	newSlice = std::max(newSlice, m_minSlice[viewIndex]);
	newSlice = std::min(newSlice, m_maxSlice[viewIndex]);

	RequestSetSlice(view, newSlice);

	if (m_renderers[viewIndex]) {
		auto* mgr = m_renderers[viewIndex]->GetOverlayManager();
		if (mgr) mgr->OnSliceChanged(view, newSlice);
		m_renderers[viewIndex]->RequestRender();
	}
}

void ThreeViewController::UpdateSliceByWorldPoint(std::array<double, 3> worldPoint)
{
	if (!m_image || m_isUpdatingSlice) return;
	m_isUpdatingSlice = true;

	double ijk[3];
	double picked[3] = { worldPoint[0], worldPoint[1], worldPoint[2] };
	m_image->TransformPhysicalPointToContinuousIndex(picked, ijk);

	SetSliceInternal(ViewType::Axial, static_cast<int>(std::round(ijk[2])));
	SetSliceInternal(ViewType::Sagittal, static_cast<int>(std::round(ijk[0])));
	SetSliceInternal(ViewType::Coronal, static_cast<int>(std::round(ijk[1])));

	m_isUpdatingSlice = false;

	for (int i = 0; i < 3; ++i) {
		if (!m_renderers[i]) continue;
		ViewType vt = static_cast<ViewType>(i);
		int currentSlice = m_renderers[i]->GetSlice();
		m_renderers[i]->GetOverlayManager()->OnSliceChanged(vt, currentSlice);
		m_renderers[i]->RequestRender();
	}
}

void ThreeViewController::SetWindowLevel(double window, double level)
{
	m_windowWidth = window;
	m_windowLevel = level;

	for (auto* r : m_renderers) {
		if (!r) continue;
		r->SetColorWindow(m_windowWidth);
		r->SetColorLevel(m_windowLevel);
		r->RequestRender();
	}
}

void ThreeViewController::Zoom(int viewIndex, double factor,
	std::array<double, 3> focalWorldPoint)
{
	auto* renderer = GetRenderer(viewIndex);
	if (!renderer) return;

	auto viewer = renderer->GetViewer();
	if (!viewer || !viewer->GetRenderer()) return;

	auto* camera = viewer->GetRenderer()->GetActiveCamera();
	if (!camera || !camera->GetParallelProjection()) return;

	auto* windowSize = viewer->GetRenderWindow()->GetSize();
	if (windowSize[0] <= 0 || windowSize[1] <= 0) return;

	double oldFocal[3], oldPos[3];
	camera->GetFocalPoint(oldFocal);
	camera->GetPosition(oldPos);

	double newFocal[3], newPos[3];
	for (int i = 0; i < 3; ++i) {
		const double shift = focalWorldPoint[i] - oldFocal[i];
		const double offset = shift * (1.0 - 1.0 / factor);
		newFocal[i] = oldFocal[i] + offset;
		newPos[i] = oldPos[i] + offset;
	}

	camera->SetFocalPoint(newFocal);
	camera->SetPosition(newPos);
	camera->SetParallelScale(camera->GetParallelScale() / factor);
	viewer->GetRenderer()->ResetCameraClippingRange();

	renderer->RequestRender();
}

// ============================================================
//  交互模式
// ============================================================

void ThreeViewController::SetInteractionMode(InteractionMode mode)
{
	if (m_currentMode == mode) return;

	// 切走时清除旧模式的绘制与进行中状态（对齐原版 uncheck → clear 语义，
	// 否则角度/长度测量切到普通模式后永远残留、无法取消）
	auto oldIt = m_strategies.find(m_currentMode);
	if (oldIt != m_strategies.end() && oldIt->second) {
		for (int i = 0; i < 3; ++i) {
			oldIt->second->Clear(i);
		}
	}

	UnregisterEventCallbacks();
	m_currentMode = mode;
	RegisterEventCallbacks();

	// ← 新增：通知新策略已激活，让其做立即初始化
	auto it = m_strategies.find(m_currentMode);
	if (it != m_strategies.end() && it->second) {
		it->second->OnActivated();
	}

	for (int i = 0; i < 3; ++i) {
		if (m_renderers[i]) m_renderers[i]->RequestRender();
	}
}

// ============================================================
//  切片操作
// ============================================================

void ThreeViewController::RequestSetSlice(ViewType view, int slice)
{
	const int idx = static_cast<int>(view);
	if (!m_renderers[idx]) return;

	slice = std::max(slice, m_minSlice[idx]);
	slice = std::min(slice, m_maxSlice[idx]);

	SetSliceInternal(view, slice);
	emit sliceChanged(idx, slice);
}

int ThreeViewController::GetSlice(ViewType view) const
{
	const int idx = static_cast<int>(view);
	if (!m_renderers[idx]) return 0;
	return m_renderers[idx]->GetSlice();
}

// ============================================================
//  其他控制
// ============================================================

std::array<double, 6> ThreeViewController::GetImageBounds() const
{
	if (!m_image) return {};
	double raw[6];
	const_cast<vtkImageData*>(m_image.Get())->GetBounds(raw);
	return { raw[0], raw[1], raw[2], raw[3], raw[4], raw[5] };
}

void ThreeViewController::ClearAllStrategyDrawings()
{
	// 「清除标注 / 模式取消选中」须清掉**所有**模式的绘制，而非仅当前模式——
	// 否则切到普通模式后再点清除，角度/长度测量永远清不掉
	for (auto& kv : m_strategies) {
		if (!kv.second) continue;
		for (int i = 0; i < 3; ++i) {
			kv.second->Clear(i);
		}
	}

	for (int i = 0; i < 3; ++i) {
		if (m_renderers[i]) m_renderers[i]->RequestRender();
	}
}

// ============================================================
//  视图重置
// ============================================================

void ThreeViewController::ResetAllViews()
{
	if (!m_image) return;

	// ── 第一步：恢复窗宽窗位 ──────────────────────────────────
	m_windowWidth = m_initialWindowWidth;
	m_windowLevel = m_initialWindowLevel;

	// ── 第二步：恢复中心切片 ──────────────────────────────────
	// 先设置切片（ResetCamera 需要图像在正确位置上才能计算边界）
	SetSliceInternal(ViewType::Axial, m_initialSlice[static_cast<int>(ViewType::Axial)]);
	SetSliceInternal(ViewType::Sagittal, m_initialSlice[static_cast<int>(ViewType::Sagittal)]);
	SetSliceInternal(ViewType::Coronal, m_initialSlice[static_cast<int>(ViewType::Coronal)]);

	// ── 第三步：逐视图重置相机并同步窗宽窗位 ────────────────────
	for (auto* r : m_renderers) {
		if (!r) continue;

		// 窗宽窗位
		r->SetColorWindow(m_windowWidth);
		r->SetColorLevel(m_windowLevel);

		// 相机：让 VTK 根据当前 Actor 包围盒自动计算最优视角
		auto viewer = r->GetViewer();
		if (viewer && viewer->GetRenderer()) {
			auto* camera = viewer->GetRenderer()->GetActiveCamera();
			// 确保仍为正交投影（缩放过程中不应改变，保险起见再设一次）
			camera->SetParallelProjection(true);
			// ResetCamera 会重置焦点、位置、平行缩放比，使图像充满视口
			viewer->GetRenderer()->ResetCamera();
		}

		r->RequestRender();
	}

	// ── 第四步：同步发出切片变化信号（更新 UI 滑块等）────────────
	emit sliceChanged(static_cast<int>(ViewType::Axial),
		m_initialSlice[static_cast<int>(ViewType::Axial)]);
	emit sliceChanged(static_cast<int>(ViewType::Sagittal),
		m_initialSlice[static_cast<int>(ViewType::Sagittal)]);
	emit sliceChanged(static_cast<int>(ViewType::Coronal),
		m_initialSlice[static_cast<int>(ViewType::Coronal)]);
}

// ============================================================
//  私有方法
// ============================================================

void ThreeViewController::ComputeSliceRanges()
{
	if (!m_image) return;

	int dims[3];
	m_image->GetDimensions(dims);

	m_minSlice[static_cast<int>(ViewType::Axial)] = 0;
	m_maxSlice[static_cast<int>(ViewType::Axial)] = dims[2] - 1;

	m_minSlice[static_cast<int>(ViewType::Sagittal)] = 0;
	m_maxSlice[static_cast<int>(ViewType::Sagittal)] = dims[0] - 1;

	m_minSlice[static_cast<int>(ViewType::Coronal)] = 0;
	m_maxSlice[static_cast<int>(ViewType::Coronal)] = dims[1] - 1;
}

void ThreeViewController::SetSliceInternal(ViewType view, int slice)
{
	const int idx = static_cast<int>(view);
	if (!m_renderers[idx]) return;

	m_renderers[idx]->SetSlice(slice);
	m_renderers[idx]->SetMaxSlice(m_maxSlice[static_cast<int>(ViewType::Axial)]);
	m_renderers[idx]->RequestRender();
}

void ThreeViewController::RegisterEventCallbacks()
{
	for (int i = 0; i < 3; ++i) {
		if (!m_renderers[i]) continue;

		if (!m_renderers[i]->GetOverlayManager()) {
			auto overlayMgr = OverlayFactory::CreateDefault();
			overlayMgr->SetImageWorldBounds(GetImageBounds());
			overlayMgr->Initialize(m_renderers[i]->GetOverlayRenderer(),
				m_renderers[i]->GetViewer());
			m_renderers[i]->SetOverlayManager(std::move(overlayMgr));
		}

		const int idx = i;

		auto wheelHandler = [this, idx](EventType type) {
			return [this, idx, type](const EventData& data) {
				auto it = m_strategies.find(InteractionMode::Normal);
				if (it != m_strategies.end() && it->second)
					it->second->HandleEvent(type, idx, data);
				};
			};

		auto modeHandler = [this, idx](EventType type) {
			return [this, idx, type](const EventData& data) {
				auto it = m_strategies.find(m_currentMode);
				if (it != m_strategies.end() && it->second)
					it->second->HandleEvent(type, idx, data);
				};
			};

		m_renderers[i]->OnEvent(EventType::WheelForward, wheelHandler(EventType::WheelForward));
		m_renderers[i]->OnEvent(EventType::WheelBackward, wheelHandler(EventType::WheelBackward));
		m_renderers[i]->OnEvent(EventType::LeftPress, modeHandler(EventType::LeftPress));
		m_renderers[i]->OnEvent(EventType::LeftMove, modeHandler(EventType::LeftMove));
		m_renderers[i]->OnEvent(EventType::LeftRelease, modeHandler(EventType::LeftRelease));
		m_renderers[i]->OnEvent(EventType::RightPress, modeHandler(EventType::RightPress));
		m_renderers[i]->OnEvent(EventType::KeyPress, modeHandler(EventType::KeyPress));
		m_renderers[i]->OnEvent(EventType::KeyRelease, modeHandler(EventType::KeyRelease));

		auto* dragStrategy = dynamic_cast<ImageDragStrategy*>(
			m_strategies[InteractionMode::ImageDrag].get());
		if (dragStrategy) {
			dragStrategy->SetDragCallback([this](int vi, double dx, double dy, double dz, double t) {
				emit imageDragUpdated(vi, dx, dy, dz, t);
				});
			dragStrategy->SetResetCallback([this]() {
				emit imageDragReset();
				});
		}
	}
}

void ThreeViewController::UnregisterEventCallbacks()
{
	for (int i = 0; i < 3; ++i) {
		if (!m_renderers[i]) continue;
		m_renderers[i]->OnEvent(EventType::WheelForward, nullptr);
		m_renderers[i]->OnEvent(EventType::WheelBackward, nullptr);
		m_renderers[i]->OnEvent(EventType::LeftPress, nullptr);
		m_renderers[i]->OnEvent(EventType::LeftMove, nullptr);
		m_renderers[i]->OnEvent(EventType::LeftRelease, nullptr);
		m_renderers[i]->OnEvent(EventType::RightPress, nullptr);
		m_renderers[i]->OnEvent(EventType::KeyPress, nullptr);
		m_renderers[i]->OnEvent(EventType::KeyRelease, nullptr);
	}
}

void ThreeViewController::SetRTStructureData(std::shared_ptr<RTStructureData> data) {
	if (!data) return;

	// 通过 IOverlayManager 接口传递数据（解析在 core，bridge 已转换）
	for (int i = 0; i < 3; ++i) {
		if (!m_renderers[i] || !m_renderers[i]->GetOverlayManager()) continue;
		m_renderers[i]->GetOverlayManager()->SetRTStructureData(data);
	}

	for (int i = 0; i < 3; ++i) {
		if (!m_renderers[i]) continue;
		ViewType vt = static_cast<ViewType>(i);
		int currentSlice = m_renderers[i]->GetSlice();
		m_renderers[i]->GetOverlayManager()->OnSliceChanged(vt, currentSlice);
		m_renderers[i]->RequestRender();
	}
}

std::vector<MeasurementRecord> ThreeViewController::CollectMeasurementRecords() const
{
	std::vector<MeasurementRecord> out;
	for (int i = 0; i < 3; ++i) {
		if (!m_renderers[i]) continue;
		auto* om = m_renderers[i]->GetOverlayManager();
		if (!om) continue;

		if (auto* dist = om->GetFeature<SimpleDistanceMeasureManager>()) {
			for (auto& r : dist->GetMeasurementRecords()) {
				r.viewIndex = i;
				out.push_back(r);
			}
		}
		if (auto* angle = om->GetFeature<SimpleAngleMeasureManager>()) {
			for (auto& r : angle->GetMeasurementRecords()) {
				r.viewIndex = i;
				out.push_back(r);
			}
		}
	}
	return out;
}

