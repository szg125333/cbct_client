#include "acquisition_panel.h"
#include "ui_acquisition_panel.h"

#include "bridge/detector_bridge.h"
#include "bridge/recipe_bridge.h"
#include "client_core.h"
#include "recipe/recipe_manager.h"
#include "ui/recipe_dialog.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QHeaderView>
#include <QMainWindow>

namespace {
const char kAddRecipeMagic[] = "__add__";
}

AcquisitionPanel::AcquisitionPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPanel)
    , m_recipeBridge(new RecipeBridge(this))
    , m_detectorBridge(new DetectorBridge(this))
{
    ui->setupUi(this);

    cbct::ClientCore::instance().initialize();

    setupRecipeCombo();
    setupRecordsTable();

    ui->lbDeviceStatus->setText(tr("设备状态：就绪（模拟）"));

    connect(ui->cbRecipe,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &AcquisitionPanel::onRecipeIndexChanged);

    connect(m_recipeBridge, &RecipeBridge::recipesUpdated,
            this, &AcquisitionPanel::setupRecipeCombo);

    connect(ui->btnStart, &QPushButton::clicked,
            this, &AcquisitionPanel::onStartClicked);

    connect(m_detectorBridge, &DetectorBridge::acquisitionFinished,
            this, [this](int total) {
                appendRecord(tr("成功"), tr("共 %1 帧").arg(total));
                ui->btnStart->setEnabled(true);
                ui->lbDeviceStatus->setText(tr("设备状态：空闲（模拟）"));
                emit acquisitionFinished(total, m_currentRecipeName);
            });

    connect(m_detectorBridge, &DetectorBridge::acquisitionError,
            this, [this](const QString &err) {
                appendRecord(tr("失败"), err);
                ui->btnStart->setEnabled(true);
                ui->lbDeviceStatus->setText(tr("设备状态：错误"));
            });

    connect(m_detectorBridge, &DetectorBridge::stateText,
            this, [this](const QString &text) {
                ui->lbDeviceStatus->setText(tr("设备状态：%1").arg(text));
            });
}

AcquisitionPanel::~AcquisitionPanel()
{
    delete ui;
}

void AcquisitionPanel::onRecipeIndexChanged(int index)
{
    if (index < 0) {
        return;
    }
    if (ui->cbRecipe->itemData(index).toString() == QLatin1String(kAddRecipeMagic)) {
        const int previous = qMax(0, index - 1);
        ui->cbRecipe->blockSignals(true);
        ui->cbRecipe->setCurrentIndex(previous);
        ui->cbRecipe->blockSignals(false);

        RecipeDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString error;
            if (m_recipeBridge->addRecipe(dialog.recipe(), &error)) {
                m_recipeBridge->refresh();
            } else {
                ui->lbRecipeSummary->setText(error);
            }
        }
        updateSummaryFromSelection();
        return;
    }

    updateSummaryFromSelection();
}

void AcquisitionPanel::onStartClicked()
{
    cbct::Recipe recipe;
    if (!currentRecipe(recipe)) {
        appendRecord(tr("失败"), tr("请选择有效配方"));
        return;
    }
    m_currentRecipeName = recipe.name;

    // 先切到 2D 采集页并完成信号连接，再启动，避免第一帧在未显示时发出
    emit acquisitionStarted();

    QString error;
    if (!m_detectorBridge->startWithRecipe(recipe, &error)) {
        appendRecord(tr("失败"), error);
        ui->lbDeviceStatus->setText(tr("设备状态：错误"));
        emit acquisitionFinished(0, m_currentRecipeName); // 让主窗口切回三视图
        return;
    }

    ui->btnStart->setEnabled(false);
}

void AcquisitionPanel::onStopClicked()
{
    stopAcquisition();
}

void AcquisitionPanel::stopAcquisition()
{
    m_detectorBridge->stop();
    ui->btnStart->setEnabled(true);
}

void AcquisitionPanel::updateSummaryFromSelection()
{
    const int index = ui->cbRecipe->currentIndex();
    if (index < 0) {
        ui->lbRecipeSummary->setText(tr("参数：kV: --    mA: --    时间: --"));
        return;
    }
    if (ui->cbRecipe->itemData(index).toString() == QLatin1String(kAddRecipeMagic)) {
        return;
    }
    const QString id = ui->cbRecipe->itemData(index).toString();
    ui->lbRecipeSummary->setText(m_recipeBridge->summaryOf(id));
}

void AcquisitionPanel::setupRecipeCombo()
{
    const QString previousId = ui->cbRecipe->currentData().toString();

    ui->cbRecipe->blockSignals(true);
    ui->cbRecipe->clear();

    const QStringList recipeNames = m_recipeBridge->names();
    for (int i = 0; i < recipeNames.size(); ++i) {
        ui->cbRecipe->addItem(recipeNames.at(i), m_recipeBridge->idAt(i));
    }

    ui->cbRecipe->insertSeparator(ui->cbRecipe->count());
    ui->cbRecipe->addItem(QStringLiteral("＋ 新增配方…"), QLatin1String(kAddRecipeMagic));

    int select = 0;
    if (!previousId.isEmpty()) {
        const int idx = ui->cbRecipe->findData(previousId);
        if (idx >= 0) {
            select = idx;
        }
    }
    ui->cbRecipe->setCurrentIndex(select);
    ui->cbRecipe->blockSignals(false);

    updateSummaryFromSelection();
}

void AcquisitionPanel::setupRecordsTable()
{
    auto *table = ui->tableRecords;
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        tr("时间"),
        tr("配方"),
        tr("状态"),
        tr("备注"),
    });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setAlternatingRowColors(true);
    table->clearContents();
    table->setRowCount(0);
}

void AcquisitionPanel::appendRecord(const QString &status, const QString &note)
{
    auto *table = ui->tableRecords;
    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    table->setItem(row, 1, new QTableWidgetItem(m_currentRecipeName));
    table->setItem(row, 2, new QTableWidgetItem(status));
    table->setItem(row, 3, new QTableWidgetItem(note));
    table->scrollToBottom();
}

bool AcquisitionPanel::currentRecipe(cbct::Recipe &out) const
{
    const int index = ui->cbRecipe->currentIndex();
    if (index < 0) {
        return false;
    }
    const QVariant data = ui->cbRecipe->itemData(index);
    if (!data.isValid() || data.toString() == QLatin1String(kAddRecipeMagic)) {
        return false;
    }
    const QString id = data.toString();
    out = cbct::ClientCore::instance().recipeManager().findById(id);
    return !out.id.isEmpty();
}
