#include "contour_list_popup.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

ContourListPopup::ContourListPopup(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::StrongFocus);
    buildUi();
}

void ContourListPopup::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    auto *titleLayout = new QHBoxLayout();
    auto *titleLabel = new QLabel(tr("轮廓列表"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11pt; color: #333;"));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto *showAllBtn = new QPushButton(tr("全显"), this);
    showAllBtn->setFixedSize(40, 22);
    connect(showAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto &kv : m_checkboxes) kv.second->setChecked(true);
        emit allVisibilityChanged(true);
    });
    auto *hideAllBtn = new QPushButton(tr("全隐"), this);
    hideAllBtn->setFixedSize(40, 22);
    connect(hideAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto &kv : m_checkboxes) kv.second->setChecked(false);
        emit allVisibilityChanged(false);
    });
    titleLayout->addWidget(showAllBtn);
    titleLayout->addWidget(hideAllBtn);
    mainLayout->addLayout(titleLayout);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QStringLiteral("color: #d0d0d0;"));
    mainLayout->addWidget(separator);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setMaximumHeight(300);

    auto *listContainer = new QWidget(this);
    m_listLayout = new QVBoxLayout(listContainer);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(2);
    m_scrollArea->setWidget(listContainer);
    mainLayout->addWidget(m_scrollArea);

    setStyleSheet(QStringLiteral(
        "ContourListPopup { background-color: #ffffff; border: 1px solid #c0c0c0;"
        " border-radius: 6px; }"));
    setMinimumWidth(220);
}

void ContourListPopup::setRoiList(const std::vector<ROIDisplayInfo> &roiList)
{
    clear();
    for (const auto &roi : roiList) {
        m_listLayout->addWidget(createRoiItem(roi));
    }
    m_listLayout->addStretch();
    adjustSize();
}

void ContourListPopup::clear()
{
    m_checkboxes.clear();
    while (m_listLayout->count() > 0) {
        auto *item = m_listLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }
}

QWidget *ContourListPopup::createRoiItem(const ROIDisplayInfo &roi)
{
    auto *itemWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    auto *colorBlock = new QLabel(itemWidget);
    colorBlock->setFixedSize(14, 14);
    const int r = static_cast<int>(roi.color[0] * 255);
    const int g = static_cast<int>(roi.color[1] * 255);
    const int b = static_cast<int>(roi.color[2] * 255);
    colorBlock->setStyleSheet(QStringLiteral(
        "background-color: rgb(%1, %2, %3); border: 1px solid #999; border-radius: 2px;")
        .arg(r).arg(g).arg(b));
    layout->addWidget(colorBlock);

    auto *checkbox = new QCheckBox(QString::fromStdString(roi.name), itemWidget);
    checkbox->setChecked(roi.visible);
    checkbox->setStyleSheet(QStringLiteral("font-size: 10pt; color: #333;"));
    const int roiNumber = roi.roiNumber;
    connect(checkbox, &QCheckBox::toggled, this,
            [this, roiNumber](bool checked) { emit roiVisibilityChanged(roiNumber, checked); });
    layout->addWidget(checkbox);
    layout->addStretch();

    m_checkboxes[roiNumber] = checkbox;
    return itemWidget;
}

void ContourListPopup::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && !isActiveWindow()) {
        hide();
    }
    QWidget::changeEvent(event);
}
