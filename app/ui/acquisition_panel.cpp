#include "acquisition_panel.h"
#include "ui_acquisition_panel.h"

#include "bridge/recipe_bridge.h"
#include "client_core.h"
#include "ui/recipe_dialog.h"

#include <QAbstractItemView>
#include <QHeaderView>

namespace {
const char kAddRecipeMagic[] = "__add__";
}

AcquisitionPanel::AcquisitionPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPanel)
    , m_recipeBridge(new RecipeBridge(this))
{
    ui->setupUi(this);

    cbct::ClientCore::instance().initialize();

    setupRecipeCombo();
    setupRecordsTable();

    ui->lbDeviceStatus->setText(tr("设备状态：未连接"));

    connect(ui->cbRecipe,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &AcquisitionPanel::onRecipeIndexChanged);

    connect(m_recipeBridge, &RecipeBridge::recipesUpdated,
            this, &AcquisitionPanel::setupRecipeCombo);
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
