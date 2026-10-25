#ifndef CBCT_RECIPE_PARAMS_H
#define CBCT_RECIPE_PARAMS_H

#include <QString>

namespace cbct {

/// 高压发生器参数
struct HvgParams {
    double tubeVoltageKv = 80.0;
    double tubeCurrentMa = 20.0;
    int exposureTimeMs = 500;
    int pulseCount = 1;
    double filamentPreheat = 0.0;
};

/// X 射线平板探测器参数
struct DetectorParams {
    int binning = 1;
    double pixelPitchMm = 0.2;
    int width = 1024;
    int height = 1024;
    int frameRateFps = 30;
    double gain = 1.0;
    double offset = 0.0;
};

/// 龙门架 / 旋转扫描参数
struct GantryParams {
    double rotationSpeedDegS = 20.0;
    double startAngleDeg = 0.0;
    double endAngleDeg = 360.0;
    int totalFrames = 600;
    double pitchMmPerRot = 0.0;
};

/// 重建算法参数
struct ReconParams {
    QString algorithm = QStringLiteral("FDK");
    double voxelSizeMm = 0.3;
    int iterations = 20;
    QString filterKernel = QStringLiteral("standard");
    bool gpuEnabled = true;
};

/// 配方总参数
struct Recipe {
    QString id;
    QString name;
    QString description;
    HvgParams hvg;
    DetectorParams detector;
    GantryParams gantry;
    ReconParams recon;
};

} // namespace cbct

#endif // CBCT_RECIPE_PARAMS_H
