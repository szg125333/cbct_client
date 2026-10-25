#ifndef LEFT_PANEL_H
#define LEFT_PANEL_H

#include <QWidget>

namespace Ui {
class LeftPanel;
}

/// 左侧停靠面板：内部为 QStackedWidget，后续可挂多个功能页
class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);
    ~LeftPanel() override;

    /// 添加一页，返回页索引
    int addPage(QWidget *page, const QString &title = QString());

    /// 切换到指定页
    void setCurrentPage(int index);

    int pageCount() const;

private:
    Ui::LeftPanel *ui;
};

#endif // LEFT_PANEL_H
