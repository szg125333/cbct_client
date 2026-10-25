#include "recipe_dialog.h"
#include "ui_recipe_dialog.h"

#include "common/log_service.h"

#include <QUuid>

RecipeDialog::RecipeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RecipeDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("新增配方"));
    resize(520, 560);

    setupSpinRanges();

    // 「重建算法」页下拉：与 ReconParams::algorithm 可选值对应
    ui->cbAlgorithm->addItems({
        QStringLiteral("FDK"),   ///< 锥束滤波反投影，最常用
        QStringLiteral("SART"),  ///< 迭代
        QStringLiteral("SIRT"),  ///< 迭代
    });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RecipeDialog::onAccept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

RecipeDialog::~RecipeDialog()
{
    delete ui;
}

cbct::Recipe RecipeDialog::recipe() const
{
    return m_recipe;
}

void RecipeDialog::onAccept()
{
    if (ui->leName->text().trimmed().isEmpty()) {
        CBCT_LOG_WARN("ui", "新增配方：名称为空，拒绝保存");
        ui->leName->setFocus();
        return;
    }
    m_recipe = collectRecipe();
    CBCT_LOG_INFO("ui", "新增配方对话框确认 name={}", m_recipe.name.toStdString());
    accept();
}

void RecipeDialog::setupSpinRanges()
{
    ui->sbVoltageKv->setRange(40.0, 150.0);
    ui->sbVoltageKv->setDecimals(1);
    ui->sbVoltageKv->setValue(80.0);

    ui->sbCurrentMa->setRange(1.0, 200.0);
    ui->sbCurrentMa->setDecimals(1);
    ui->sbCurrentMa->setValue(20.0);

    ui->sbExposureTimeMs->setRange(1, 10000);
    ui->sbExposureTimeMs->setValue(500);

    ui->sbPulseCount->setRange(1, 100);
    ui->sbPulseCount->setValue(1);

    ui->sbBinning->setRange(1, 8);
    ui->sbBinning->setValue(1);

    ui->sbPixelPitch->setRange(0.05, 2.0);
    ui->sbPixelPitch->setDecimals(3);
    ui->sbPixelPitch->setValue(0.2);

    ui->sbWidth->setRange(64, 4096);
    ui->sbWidth->setValue(1024);
    ui->sbHeight->setRange(64, 4096);
    ui->sbHeight->setValue(1024);

    ui->sbFrameRate->setRange(1, 120);
    ui->sbFrameRate->setValue(30);

    ui->sbGain->setRange(0.0, 10.0);
    ui->sbGain->setDecimals(3);
    ui->sbGain->setValue(1.0);

    ui->sbOffset->setRange(-1000.0, 1000.0);
    ui->sbOffset->setDecimals(2);
    ui->sbOffset->setValue(0.0);

    ui->sbRotSpeed->setRange(0.1, 180.0);
    ui->sbRotSpeed->setDecimals(2);
    ui->sbRotSpeed->setValue(20.0);

    ui->sbStartAngle->setRange(0.0, 360.0);
    ui->sbStartAngle->setDecimals(1);
    ui->sbStartAngle->setValue(0.0);

    ui->sbEndAngle->setRange(0.0, 720.0);
    ui->sbEndAngle->setDecimals(1);
    ui->sbEndAngle->setValue(360.0);

    ui->sbTotalFrames->setRange(1, 100000);
    ui->sbTotalFrames->setValue(600);

    ui->sbPitch->setRange(0.0, 500.0);
    ui->sbPitch->setDecimals(2);
    ui->sbPitch->setValue(0.0);

    // ---- 重建算法页（tabRecon）控件范围 ----
    // 体素边长：过细则内存爆炸，过粗则空间分辨率差
    ui->sbVoxelSize->setRange(0.05, 5.0);
    ui->sbVoxelSize->setDecimals(3);
    ui->sbVoxelSize->setValue(0.3); // 默认 0.3 mm

    // 迭代次数：仅 SART/SIRT 有意义；FDK 可不管
    ui->sbIterations->setRange(1, 500);
    ui->sbIterations->setValue(20);

    ui->chkGpu->setChecked(true); // 默认尝试 GPU 重建
    ui->leFilterKernel->setText(QStringLiteral("standard")); // 默认滤波核
}

cbct::Recipe RecipeDialog::collectRecipe() const
{
    cbct::Recipe r;
    r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.name = ui->leName->text().trimmed();
    r.description = ui->leDescription->text().trimmed();

    r.hvg.tubeVoltageKv = ui->sbVoltageKv->value();
    r.hvg.tubeCurrentMa = ui->sbCurrentMa->value();
    r.hvg.exposureTimeMs = ui->sbExposureTimeMs->value();
    r.hvg.pulseCount = ui->sbPulseCount->value();

    r.detector.binning = ui->sbBinning->value();
    r.detector.pixelPitchMm = ui->sbPixelPitch->value();
    r.detector.width = ui->sbWidth->value();
    r.detector.height = ui->sbHeight->value();
    r.detector.frameRateFps = ui->sbFrameRate->value();
    r.detector.gain = ui->sbGain->value();
    r.detector.offset = ui->sbOffset->value();

    r.gantry.rotationSpeedDegS = ui->sbRotSpeed->value();
    r.gantry.startAngleDeg = ui->sbStartAngle->value();
    r.gantry.endAngleDeg = ui->sbEndAngle->value();
    r.gantry.totalFrames = ui->sbTotalFrames->value();
    r.gantry.pitchMmPerRot = ui->sbPitch->value();

    // ---- 重建算法页 → ReconParams ----
    r.recon.algorithm = ui->cbAlgorithm->currentText();       // FDK/SART/SIRT
    r.recon.voxelSizeMm = ui->sbVoxelSize->value();           // 体素 mm
    r.recon.iterations = ui->sbIterations->value();           // 迭代次数
    r.recon.filterKernel = ui->leFilterKernel->text().trimmed(); // 滤波核
    r.recon.gpuEnabled = ui->chkGpu->isChecked();             // GPU 开关

    return r;
}
