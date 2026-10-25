/**
 * @file main_toolbar.h
 * @brief 顶部工具栏：6 组 QToolButton（纯图标 + ToolTip），嵌在菜单栏下方
 */
#ifndef MAIN_TOOLBAR_H
#define MAIN_TOOLBAR_H

#include <QHash>
#include <QIcon>
#include <QWidget>

namespace Ui {
class MainToolbar;
}

/**
 * @class MainToolbar
 * @brief 按钮 objectName → 中文功能名 / 图标资源路径 的映射在构造时建立
 */
class MainToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit MainToolbar(QWidget *parent = nullptr);
    ~MainToolbar() override;

    /**
     * @brief 覆盖某按钮图标
     * @param actionName 可用 objectName（"btnOpen"）或中文名（"打开文件"）
     */
    void setButtonIcon(const QString &actionName, const QIcon &icon);

signals:
    /// 点击任一登记过的按钮时发出（参数为中文功能名）
    void actionTriggered(const QString &actionName);

private slots:
    void onToolButtonClicked(); ///< sender() → 查表 → 打印并 emit

private:
    void setupConnections();       ///< 统一设为仅图标、连 clicked
    void applyIconsAndTooltips();  ///< 从 qrc 加载 PNG，设 ToolTip=中文名
    QString actionNameOf(QWidget *button) const; ///< objectName → 中文名

    Ui::MainToolbar *ui;
    QHash<QString, QString> m_objectToAction;   ///< "btnOpen" → "打开文件"
    QHash<QString, QString> m_objectToIconPath; ///< "btnOpen" → ":/icons/open.png"
};

#endif // MAIN_TOOLBAR_H
