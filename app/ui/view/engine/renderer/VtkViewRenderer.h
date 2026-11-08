#pragma once

#include "interface/IViewRenderer.h"
#include "common/RenderViewState.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vtkSmartPointer.h>
#include <vtkObject.h>

class QVTKOpenGLNativeWidget;
class vtkImageViewer2;
class vtkGenericOpenGLRenderWindow;
class vtkCallbackCommand;
class vtkImageData;
class vtkRenderer;
class IOverlayManager;

/**
 * @brief VTK 切片视图渲染器
 *
 * 封装单个切片视图（轴状 / 矢状 / 冠状）的完整渲染逻辑：
 *   - 使用 vtkImageViewer2 显示 DICOM 图像切片
 *   - 在独立的第二层渲染器（Overlay）上绘制测量标注
 *   - 拦截 VTK 鼠标 / 键盘事件并转发给上层 Strategy
 *   - 通过 QTimer 合并高频渲染请求，16 ms 内只渲染一次
 */
class VtkViewRenderer : public QObject, public IViewRenderer {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param widget 承载此视图的 Qt VTK 渲染窗口，不可为 nullptr
     */
    explicit VtkViewRenderer(QVTKOpenGLNativeWidget* widget);
    ~VtkViewRenderer() override;

    // ----------------------------------------------------------------
    //  IViewRenderer 接口实现
    // ----------------------------------------------------------------

    void SetInputData(vtkImageData* image)              override;
    void SetOrientation(ViewType viewType)              override;
    void SetSlice(int slice)                            override;
    void SetMaxSlice(int maxSlice)                      override;
    int  GetSlice() const                               override;
    ViewType GetCurrentViewType()                       override;
    void SetColorWindow(double window)                  override;
    void SetColorLevel(double level)                    override;

    std::array<double, 3> PickWorldPosition(int screenX, int screenY) override;
    void SetCurrentClickWorldPos(std::array<double, 3> worldPos)      override;

    void RequestRender()                                override;
    void UpdateBasicInfoActor()                         override;

    vtkSmartPointer<vtkImageViewer2> GetViewer()        override { return m_viewer; }
    vtkSmartPointer<vtkRenderer>     GetOverlayRenderer() override { return m_overlayRenderer; }
    IOverlayManager* GetOverlayManager()                override { return m_overlayManager.get(); }
    void SetOverlayManager(std::unique_ptr<IOverlayManager> manager) override;

    void OnEvent(EventType type, std::function<void(const EventData&)> cb) override;

    // ----------------------------------------------------------------
    //  状态查询（供 Overlay 信息层读取）
    // ----------------------------------------------------------------

    double   GetColorWindow()    const { return m_windowWidth; }
    double   GetColorLevel()     const { return m_windowLevel; }
    int      GetCurrentSlice()   const { return m_currentSlice; }
    int      GetMaxSlices()      const { return m_maxSlices; }
    //ViewType GetCurrentViewType()const { return m_currentViewType; }
    std::array<double, 3> GetCurrentClickWorldPos() const { return m_currentClickWorldPos; }

signals:
    /// @brief 视图状态发生变化时发出，Overlay 信息层监听此信号
    void viewStateChanged(const RenderViewState& state);

private slots:
    /// @brief 由 QTimer 触发，执行真实渲染
    void OnRenderTimerTimeout();

    /// @brief 接收 viewStateChanged 信号，转发给 Overlay 管理器
    void OnViewStateChanged(const RenderViewState& state);

private:
    // ----------------------------------------------------------------
    //  VTK 事件回调（静态，避免 VTK 与 Qt 对象生命周期耦合）
    // ----------------------------------------------------------------

    /**
     * @brief VTK 通用回调入口
     *
     * 将 VTK 鼠标 / 键盘事件转换为 EventData，
     * 再调用 m_callbacks 中对应的回调函数。
     */
    static void VtkEventCallback(vtkObject* caller, unsigned long eventId,
        void* clientData, void* callData);

    // ----------------------------------------------------------------
    //  成员变量 —— Qt
    // ----------------------------------------------------------------

    /// 承载渲染窗口的 Qt Widget（弱引用，防止悬空指针）
    QPointer<QVTKOpenGLNativeWidget> m_widget;

    /// 渲染合并计时器，16 ms 单次触发
    QTimer m_renderTimer;

    // ----------------------------------------------------------------
    //  成员变量 —— VTK
    // ----------------------------------------------------------------

    vtkSmartPointer<vtkImageViewer2>           m_viewer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;

    /// Overlay 独立渲染器（第二层，不影响图像渲染）
    vtkSmartPointer<vtkRenderer>               m_overlayRenderer;

    /// VTK 事件监听命令
    vtkSmartPointer<vtkCallbackCommand>        m_vtkCmd;

    /// 事件类型 → 回调函数 映射表
    std::unordered_map<EventType, std::function<void(const EventData&)>> m_callbacks;

    // ----------------------------------------------------------------
    //  成员变量 —— Overlay
    // ----------------------------------------------------------------

    std::unique_ptr<IOverlayManager> m_overlayManager;

    // ----------------------------------------------------------------
    //  成员变量 —— 状态缓存
    // ----------------------------------------------------------------

    ViewType             m_currentViewType = ViewType::None;
    double               m_windowWidth = 0.0;
    double               m_windowLevel = 0.0;
    int                  m_currentSlice = 0;
    int                  m_maxSlices = 0;
    std::array<double, 3> m_currentClickWorldPos = { 0.0, 0.0, 0.0 };
};
