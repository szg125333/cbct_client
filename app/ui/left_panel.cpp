#include "left_panel.h"
#include "ui_left_panel.h"
#include "acquisition_panel.h"

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LeftPanel)
{
    ui->setupUi(this);

    // 第一页：采集启动
    auto *acquisition = new AcquisitionPanel(ui->stackWidget);
    ui->stackWidget->addWidget(acquisition);
    ui->stackWidget->setCurrentIndex(0);
}

LeftPanel::~LeftPanel()
{
    delete ui;
}

int LeftPanel::addPage(QWidget *page, const QString &title)
{
    Q_UNUSED(title)
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
