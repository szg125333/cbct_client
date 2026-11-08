#pragma once

#include "interface/IViewController.h"
#include "common/InteractionMode.h"
#include "common/ViewTypes.h"
#include "common/Measurement/MeasurementTypes.h"
#include "utils/RTStructureData.h"

#include <QObject>
#include <array>
#include <map>
#include <memory>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

class IInteractionStrategy;
class vtkImageData;

/**
 * @brief 三视图控制器
 *
 * 管理轴状（Axial）、矢状（Sagittal）、冠状（Coronal）三个切片视图的
 * 全局状态协调，包括：
 *   - 图像数据绑定与初始化
 *   - 切片同步
 *   - 窗宽窗位广播
 *   - 交互模式切换
 *   - 视图缩放
 *   - 视图重置（恢复初始相机/窗位/切片）
 */
class ThreeViewController : public QObject, public IViewController {
    Q_OBJECT

public:
    explicit ThreeViewController(QObject* parent = nullptr);
    ~ThreeViewController() override;

    // ----------------------------------------------------------------
    //  初始化
    // ----------------------------------------------------------------

    void SetRenderers(std::array<IViewRenderer*, 3> renderers);
    void SetImageData(vtkImageData* image);
    //void LoadContourData(std::vector<RtRoi> rois);

    // ----------------------------------------------------------------
    //  IViewController 接口
    // ----------------------------------------------------------------

    void ChangeSlice(int viewIndex, int delta)                     override;
    void UpdateSliceByWorldPoint(std::array<double, 3> worldPoint) override;
    double GetWindowWidth() const                                  override { return m_windowWidth; }
    double GetWindowLevel() const                                  override { return m_windowLevel; }
    void SetWindowLevel(double window, double level)               override;
    void Zoom(int viewIndex, double factor,
        std::array<double, 3> focalWorldPoint)               override;
    IViewRenderer* GetRenderer(int viewIndex)                      override;
    const vtkImageData* GetImage() const                           override;

    // ----------------------------------------------------------------
    //  交互模式
    // ----------------------------------------------------------------

    void SetInteractionMode(InteractionMode mode);
    InteractionMode GetInteractionMode() const { return m_currentMode; }

    // ----------------------------------------------------------------
    //  切片操作
    // ----------------------------------------------------------------

    void RequestSetSlice(ViewType view, int slice);
    int  GetSlice(ViewType view) const;

    // ----------------------------------------------------------------
    //  其他控制
    // ----------------------------------------------------------------

    std::array<double, 6> GetImageBounds() const;
    void ClearAllStrategyDrawings();

    // ----------------------------------------------------------------
    //  视图重置
    // ----------------------------------------------------------------

    /**
     * @brief 重置三视图到加载图像时的初始状态
     *
     * 重置内容：
     *   1. 相机   → ResetCamera()，图像重新充满视口
     *   2. 窗宽窗位 → 恢复图像灰度范围推算的初始值
     *   3. 切片   → 恢复到图像中心切片
     *
     * 不会重置 Overlay 标注和当前交互模式。
     */
    void ResetAllViews();

    // 设置 RTSTRUCT 轮廓数据并在三视图中显示（解析在 core/dicom，bridge 转换后传入）
    void SetRTStructureData(std::shared_ptr<RTStructureData> data);

    /// 导出用：汇总三个视图的全部距离/角度测量
    std::vector<MeasurementRecord> CollectMeasurementRecords() const;
signals:
    void sliceChanged(int viewIndex, int slice);
    // 新增：图像拖动信号，转发给 UI 层
    void imageDragUpdated(int viewIndex,
        double dx, double dy, double dz,
        double totalDist);
    void imageDragReset();

private:

    void ComputeSliceRanges();
    void SetSliceInternal(ViewType view, int slice);
    void RegisterEventCallbacks();
    void UnregisterEventCallbacks();

    // ----------------------------------------------------------------
    //  成员变量
    // ----------------------------------------------------------------

    std::array<IViewRenderer*, 3> m_renderers = { nullptr, nullptr, nullptr };
    vtkWeakPointer<vtkImageData>  m_image;

    // 当前窗宽窗位
    double m_windowWidth = 400.0;
    double m_windowLevel = 40.0;

    // 初始窗宽窗位（SetImageData 时缓存，ResetAllViews 时还原）
    double m_initialWindowWidth = 400.0;
    double m_initialWindowLevel = 40.0;

    int m_minSlice[3] = { 0, 0, 0 };
    int m_maxSlice[3] = { 0, 0, 0 };

    // 初始中心切片（SetImageData 时缓存，ResetAllViews 时还原）
    int m_initialSlice[3] = { 0, 0, 0 };

    InteractionMode m_currentMode = InteractionMode::Normal;
    std::map<InteractionMode, std::unique_ptr<IInteractionStrategy>> m_strategies;

    bool m_isUpdatingSlice = false;
};
