#ifndef ACQUISITION_PANEL_H
#define ACQUISITION_PANEL_H

#include <QWidget>

class RecipeBridge;

namespace Ui {
class AcquisitionPanel;
}

/// 左侧第一页：CBCT 采集启动界面
class AcquisitionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AcquisitionPanel(QWidget *parent = nullptr);
    ~AcquisitionPanel() override;

private slots:
    void onRecipeIndexChanged(int index);
    void setupRecipeCombo();

private:
    void setupRecordsTable();
    void updateSummaryFromSelection();

    Ui::AcquisitionPanel *ui;
    RecipeBridge *m_recipeBridge = nullptr;
};

#endif // ACQUISITION_PANEL_H
