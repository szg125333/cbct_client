#include "left_panel.h"
#include "ui_left_panel.h"
#include "acquisition_panel.h" // ui/acquisition/
#include "common/log_service.h"

#include <QHeaderView>
#include <QTableWidget>

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LeftPanel)
{
    ui->setupUi(this);
    CBCT_LOG_DEBUG("ui", "左侧功能栏初始化");

    // ui->stackWidget：左侧功能页栈；宽约 220–420（见 left_panel.ui）
    // 第 0 页：采集启动界面（配方下拉、开始采集、检测记录表）
    m_acquisition = new AcquisitionPanel(ui->stackWidget);
    ui->stackWidget->addWidget(m_acquisition);

    // 第 1 页：DICOM 元数据表（移植自 DicomImageStudy LeftToolWidget::SetDicomMetadata）
    m_metaTable = new QTableWidget(ui->stackWidget);
    m_metaTable->setColumnCount(2);
    m_metaTable->setHorizontalHeaderLabels({tr("标签"), tr("值")});
    m_metaTable->horizontalHeader()->setStretchLastSection(true);
    m_metaTable->verticalHeader()->setVisible(false);
    m_metaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metaTable->setSelectionMode(QAbstractItemView::NoSelection);
    ui->stackWidget->addWidget(m_metaTable);

    ui->stackWidget->setCurrentIndex(0);
}

LeftPanel::~LeftPanel()
{
    delete ui;
}

void LeftPanel::setDicomMetadata(const std::map<std::string, std::string> &metadata)
{
    m_metaTable->setRowCount(static_cast<int>(metadata.size()));
    int row = 0;
    for (const auto &kv : metadata) {
        if (kv.second.empty()) continue;
        m_metaTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(kv.first)));
        m_metaTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(kv.second)));
        ++row;
    }
    m_metaTable->setRowCount(row);  // 跳过空值后收缩
    // 默认展示元数据页（加载图像后用户最先想看患者/序列信息）
    ui->stackWidget->setCurrentWidget(m_metaTable);
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
