/**
 * @file left_panel.h
 * @brief 主界面左侧「功能停靠区」：QStackedWidget 多功能页容器
 *
 * 说明：当前用固定宽度侧栏 + Stack，而不是 QDockWidget。
 * 若日后需要可拖拽停靠，可把本容器换成 QDockWidget，对外接口可保持类似。
 *
 * 页规划：
 *   index 0  AcquisitionPanel  采集启动（配方/开始/记录）—— 已挂接
 *   index 1+ 预留：患者列表、后处理、设置等，addPage 即可
 */
#ifndef LEFT_PANEL_H
#define LEFT_PANEL_H

#include <QWidget>
#include <map>
#include <string>

class AcquisitionPanel;
class QTableWidget;

namespace Ui {
class LeftPanel;
}

/**
 * @class LeftPanel
 * @brief 左侧停靠式功能栏；主窗口通过 acquisitionPanel() 访问采集页
 */
class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);
    ~LeftPanel() override;

    /// 采集页指针（主窗口连开始/结束、帧队列用）；未创建则为 nullptr
    AcquisitionPanel *acquisitionPanel() const { return m_acquisition; }

    /// DICOM 元数据表（第 1 页）：key/value 双列
    void setDicomMetadata(const std::map<std::string, std::string> &metadata);

    /// 追加一页到 stack（title 暂未用于 UI 显示，可留给将来页签）
    int addPage(QWidget *page, const QString &title = QString());

    /// 切换功能页（0=采集，其余以后定义）
    void setCurrentPage(int index);

    int pageCount() const;

private:
    Ui::LeftPanel *ui;
    AcquisitionPanel *m_acquisition = nullptr; ///< 第 0 页：采集启动
    QTableWidget *m_metaTable = nullptr;      ///< 第 1 页：DICOM 元数据
};

#endif // LEFT_PANEL_H
