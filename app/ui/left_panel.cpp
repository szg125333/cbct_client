#include "left_panel.h"
#include "ui_left_panel.h"
#include "acquisition_panel.h"

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LeftPanel)
{
    ui->setupUi(this);

    // ui->stackWidget：左侧功能页栈；宽约 220–420（见 left_panel.ui）
    // 第 0 页：采集启动界面（配方下拉、开始采集、检测记录表）
    m_acquisition = new AcquisitionPanel(ui->stackWidget);
    ui->stackWidget->addWidget(m_acquisition);
    ui->stackWidget->setCurrentIndex(0);

    // 后续功能页示例：
    //   auto *patients = new PatientPanel(ui->stackWidget);
    //   ui->stackWidget->addWidget(patients);
    // 再 setCurrentPage 切换；「窗口」菜单可控制本侧栏显隐
}

LeftPanel::~LeftPanel()
{
    delete ui;
}

int LeftPanel::addPage(QWidget *page, const QString &title)
{
    Q_UNUSED(title) // 预留：将来可用 title 做侧栏页签或菜单项文本
    return ui->stackWidget->addWidget(page);
}

void LeftPanel::setCurrentPage(int index)
{
    ui->stackWidget->setCurrentIndex(index);
}

int LeftPanel::pageCount() const
{
    return ui->stackWidget->count();
}
