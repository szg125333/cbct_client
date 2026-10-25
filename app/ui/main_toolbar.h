#ifndef MAIN_TOOLBAR_H
#define MAIN_TOOLBAR_H

#include <QIcon>
#include <QWidget>

namespace Ui {
class MainToolbar;
}

class MainToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit MainToolbar(QWidget *parent = nullptr);
    ~MainToolbar() override;

    /// 按 actionName 设置按钮图标（objectName，如 "btnOpen"）
    void setButtonIcon(const QString &actionName, const QIcon &icon);

signals:
    void actionTriggered(const QString &actionName);

private slots:
    void onToolButtonClicked();

private:
    void setupConnections();
    void applyIconsAndTooltips();
    QString actionNameOf(QWidget *button) const;

    Ui::MainToolbar *ui;
    QHash<QString, QString> m_objectToAction;
    QHash<QString, QString> m_objectToIconPath;
};

#endif // MAIN_TOOLBAR_H
