#include "left_panel.h"
#include "ui_left_panel.h"

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LeftPanel)
{
    ui->setupUi(this);

    // 暂时只有一页空白界面，后续再往 stackWidget 里加页
    auto *blank = new QWidget(ui->stackWidget);
    blank->setObjectName(QStringLiteral("blankPage"));
    ui->stackWidget->addWidget(blank);
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
