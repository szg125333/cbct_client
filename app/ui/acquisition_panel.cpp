#include "acquisition_panel.h"
#include "ui_acquisition_panel.h"

#include "bridge/detector_bridge.h"
#include "bridge/record_bridge.h"
#include "bridge/recipe_bridge.h"
#include "client_core.h"
#include "common/app_paths.h"
#include "db/database_service.h"
#include "recipe/recipe_manager.h"
#include "ui/recipe_dialog.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDebug>
#include <QHeaderView>
#include <QMainWindow>
#include <QTableWidgetItem>

namespace {
const char kAddRecipeMagic[] = "__add__";
}

AcquisitionPanel::AcquisitionPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPanel)
    , m_recipeBridge(new RecipeBridge(this))
    , m_detectorBridge(new DetectorBridge(this))
    , m_recordBridge(new RecordBridge(this))
{
    ui->setupUi(this);

    setupRecipeCombo();
    setupRecordsTable();

    ui->lbDeviceStatus->setText(tr("设备状态：就绪（模拟）"));

    // ---- 先连信号，再 initialize（避免初始化失败时错过 dbError）----
    connect(m_recordBridge, &RecordBridge::recordsUpdated,
            this, &AcquisitionPanel::fillRecordsTable);
    connect(m_recordBridge, &RecordBridge::databaseError,
            this, &AcquisitionPanel::showDatabaseError);

    connect(ui->cbRecipe,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &AcquisitionPanel::onRecipeIndexChanged);

    connect(m_recipeBridge, &RecipeBridge::recipesUpdated,
            this, &AcquisitionPanel::setupRecipeCombo);

    connect(ui->btnStart, &QPushButton::clicked,
            this, &AcquisitionPanel::onStartClicked);

    // ---- 采集结束 → 写库 → 等 recordsUpdated 刷表 ----
    connect(m_detectorBridge, &DetectorBridge::acquisitionFinished,
            this, [this](int total) {
                persistAndRefresh(makeRecord(tr("成功"), tr("共 %1 帧").arg(total), total));
                ui->btnStart->setEnabled(true);
                ui->lbDeviceStatus->setText(tr("设备状态：空闲（模拟）"));
                emit acquisitionFinished(total, m_currentRecipeName);
            });

    connect(m_detectorBridge, &DetectorBridge::acquisitionError,
            this, [this](const QString &err) {
                persistAndRefresh(makeRecord(tr("失败"), err, 0));
                ui->btnStart->setEnabled(true);
                ui->lbDeviceStatus->setText(tr("设备状态：错误"));
            });

    connect(m_detectorBridge, &DetectorBridge::stateText,
            this, [this](const QString &text) {
                ui->lbDeviceStatus->setText(tr("设备状态：%1").arg(text));
            });

    // ClientCore::initialize 会打开 SQLite；失败经 databaseError 提示
    cbct::ClientCore::instance().initialize();
    checkDatabaseOnStartup();

    // 启动加载历史检测记录（与采集结束后的加载共用 fillRecordsTable）
    if (cbct::ClientCore::instance().database().isReady()) {
        m_recordBridge->reloadRecent(100);
    }
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
        // 启动失败也记入数据库
        m_currentRecipeId.clear();
        m_currentRecipeName = tr("（未选择）");
        persistAndRefresh(makeRecord(tr("失败"), tr("请选择有效配方"), 0));
        return;
    }
    m_currentRecipeId = recipe.id;
    m_currentRecipeName = recipe.name;

    emit acquisitionStarted();

    QString error;
    if (!m_detectorBridge->startWithRecipe(recipe, &error)) {
        persistAndRefresh(makeRecord(tr("失败"), error, 0));
        ui->lbDeviceStatus->setText(tr("设备状态：错误"));
        emit acquisitionFinished(0, m_currentRecipeName);
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
    m_currentRecipeId = id;
    const cbct::Recipe r = cbct::ClientCore::instance().recipeManager().findById(id);
    m_currentRecipeName = r.name;
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
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({
        tr("时间"),
        tr("配方"),
        tr("状态"),
        tr("帧数"),
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

void AcquisitionPanel::showDatabaseError(const QString &message)
{
    // 左侧两处同时提示，方便扫一眼能看见
    ui->lbDeviceStatus->setText(tr("设备状态：数据库错误"));
    ui->lbRecipeSummary->setText(tr("数据库错误：%1").arg(message));
    qWarning().noquote() << QStringLiteral("[AcquisitionPanel] 数据库错误: %1").arg(message);
}

void AcquisitionPanel::checkDatabaseOnStartup()
{
    auto &db = cbct::ClientCore::instance().database();
    if (db.isReady()) {
        return;
    }
    const QString path = cbct::paths::databaseFilePath();
    showDatabaseError(
        tr("初始化失败，无法读取历史记录。请检查文件可写：%1").arg(path));
}

void AcquisitionPanel::fillRecordsTable(const QList<cbct::DetectionRecord> &list)
{
    auto *table = ui->tableRecords;
    table->setRowCount(0);
    for (const cbct::DetectionRecord &r : list) {
        const int row = table->rowCount();
        table->insertRow(row);
        const QString timeText = r.finishedAt.isValid()
            ? r.finishedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("—");
        table->setItem(row, 0, new QTableWidgetItem(timeText));
        table->setItem(row, 1, new QTableWidgetItem(r.recipeName));
        table->setItem(row, 2, new QTableWidgetItem(r.status));
        table->setItem(row, 3, new QTableWidgetItem(QString::number(r.frameCount)));
        table->setItem(row, 4, new QTableWidgetItem(r.note));
    }
    table->scrollToBottom();
}

cbct::DetectionRecord AcquisitionPanel::makeRecord(const QString &status,
                                                   const QString &note,
                                                   int frames) const
{
    cbct::DetectionRecord rec;
    rec.recipeId = m_currentRecipeId;
    rec.recipeName = m_currentRecipeName;
    rec.status = status;
    rec.note = note;
    rec.frameCount = frames;
    rec.finishedAt = QDateTime::currentDateTime();
    return rec;
}

void AcquisitionPanel::persistAndRefresh(const cbct::DetectionRecord &record)
{
    // 1) 异步 INSERT（db 线程）
    m_recordBridge->appendRecord(record);
    // 2) 插入完成后由 DB 再 query？简单可靠：立刻再查一次最近列表刷表
    //    与 INSERT 并行时可能略早，故在 recordInserted 后再查更稳。
    //    这里再 reload 一次；DB 任务队列保证 INSERT 先于本次 SELECT。
    m_recordBridge->reloadRecent(100);
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
