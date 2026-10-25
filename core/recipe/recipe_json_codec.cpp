#include "recipe/recipe_json_codec.h"

#include <QJsonValue>

namespace cbct {

namespace {

QJsonObject hvgToJson(const HvgParams &p)
{
    QJsonObject o;
    o[QStringLiteral("tubeVoltageKv")] = p.tubeVoltageKv;
    o[QStringLiteral("tubeCurrentMa")] = p.tubeCurrentMa;
    o[QStringLiteral("exposureTimeMs")] = p.exposureTimeMs;
    o[QStringLiteral("pulseCount")] = p.pulseCount;
    o[QStringLiteral("filamentPreheat")] = p.filamentPreheat;
    return o;
}

HvgParams hvgFromJson(const QJsonObject &o)
{
    HvgParams p;
    p.tubeVoltageKv = o.value(QStringLiteral("tubeVoltageKv")).toDouble(p.tubeVoltageKv);
    p.tubeCurrentMa = o.value(QStringLiteral("tubeCurrentMa")).toDouble(p.tubeCurrentMa);
    p.exposureTimeMs = o.value(QStringLiteral("exposureTimeMs")).toInt(p.exposureTimeMs);
    p.pulseCount = o.value(QStringLiteral("pulseCount")).toInt(p.pulseCount);
    p.filamentPreheat = o.value(QStringLiteral("filamentPreheat")).toDouble(p.filamentPreheat);
    return p;
}

QJsonObject detectorToJson(const DetectorParams &p)
{
    QJsonObject o;
    o[QStringLiteral("binning")] = p.binning;
    o[QStringLiteral("pixelPitchMm")] = p.pixelPitchMm;
    o[QStringLiteral("width")] = p.width;
    o[QStringLiteral("height")] = p.height;
    o[QStringLiteral("frameRateFps")] = p.frameRateFps;
    o[QStringLiteral("gain")] = p.gain;
    o[QStringLiteral("offset")] = p.offset;
    return o;
}

DetectorParams detectorFromJson(const QJsonObject &o)
{
    DetectorParams p;
    p.binning = o.value(QStringLiteral("binning")).toInt(p.binning);
    p.pixelPitchMm = o.value(QStringLiteral("pixelPitchMm")).toDouble(p.pixelPitchMm);
    p.width = o.value(QStringLiteral("width")).toInt(p.width);
    p.height = o.value(QStringLiteral("height")).toInt(p.height);
    p.frameRateFps = o.value(QStringLiteral("frameRateFps")).toInt(p.frameRateFps);
    p.gain = o.value(QStringLiteral("gain")).toDouble(p.gain);
    p.offset = o.value(QStringLiteral("offset")).toDouble(p.offset);
    return p;
}

QJsonObject gantryToJson(const GantryParams &p)
{
    QJsonObject o;
    o[QStringLiteral("rotationSpeedDegS")] = p.rotationSpeedDegS;
    o[QStringLiteral("startAngleDeg")] = p.startAngleDeg;
    o[QStringLiteral("endAngleDeg")] = p.endAngleDeg;
    o[QStringLiteral("totalFrames")] = p.totalFrames;
    o[QStringLiteral("pitchMmPerRot")] = p.pitchMmPerRot;
    return o;
}

GantryParams gantryFromJson(const QJsonObject &o)
{
    GantryParams p;
    p.rotationSpeedDegS = o.value(QStringLiteral("rotationSpeedDegS")).toDouble(p.rotationSpeedDegS);
    p.startAngleDeg = o.value(QStringLiteral("startAngleDeg")).toDouble(p.startAngleDeg);
    p.endAngleDeg = o.value(QStringLiteral("endAngleDeg")).toDouble(p.endAngleDeg);
    p.totalFrames = o.value(QStringLiteral("totalFrames")).toInt(p.totalFrames);
    p.pitchMmPerRot = o.value(QStringLiteral("pitchMmPerRot")).toDouble(p.pitchMmPerRot);
    return p;
}

/// ReconParams → JSON 对象（键名与 recipe.json 中 "recon" 子节点一致）
QJsonObject reconToJson(const ReconParams &p)
{
    QJsonObject o;
    o[QStringLiteral("algorithm")] = p.algorithm;       ///< 算法名
    o[QStringLiteral("voxelSizeMm")] = p.voxelSizeMm;  ///< 体素边长 mm
    o[QStringLiteral("iterations")] = p.iterations;    ///< 迭代次数（迭代算法）
    o[QStringLiteral("filterKernel")] = p.filterKernel;///< 滤波核
    o[QStringLiteral("gpuEnabled")] = p.gpuEnabled;    ///< 是否 GPU
    return o;
}

/// JSON 对象 → ReconParams；缺字段时保留结构体默认值
ReconParams reconFromJson(const QJsonObject &o)
{
    ReconParams p;
    p.algorithm = o.value(QStringLiteral("algorithm")).toString(p.algorithm);
    p.voxelSizeMm = o.value(QStringLiteral("voxelSizeMm")).toDouble(p.voxelSizeMm);
    p.iterations = o.value(QStringLiteral("iterations")).toInt(p.iterations);
    p.filterKernel = o.value(QStringLiteral("filterKernel")).toString(p.filterKernel);
    p.gpuEnabled = o.value(QStringLiteral("gpuEnabled")).toBool(p.gpuEnabled);
    return p;
}

} // namespace

QJsonObject RecipeJsonCodec::toJson(const Recipe &recipe)
{
    QJsonObject o;
    o[QStringLiteral("id")] = recipe.id;
    o[QStringLiteral("name")] = recipe.name;
    o[QStringLiteral("description")] = recipe.description;
    o[QStringLiteral("hvg")] = hvgToJson(recipe.hvg);
    o[QStringLiteral("detector")] = detectorToJson(recipe.detector);
    o[QStringLiteral("gantry")] = gantryToJson(recipe.gantry);
    o[QStringLiteral("recon")] = reconToJson(recipe.recon);
    return o;
}

Recipe RecipeJsonCodec::fromJson(const QJsonObject &object, bool *ok)
{
    Recipe r;
    r.id = object.value(QStringLiteral("id")).toString();
    r.name = object.value(QStringLiteral("name")).toString();
    r.description = object.value(QStringLiteral("description")).toString();
    r.hvg = hvgFromJson(object.value(QStringLiteral("hvg")).toObject());
    r.detector = detectorFromJson(object.value(QStringLiteral("detector")).toObject());
    r.gantry = gantryFromJson(object.value(QStringLiteral("gantry")).toObject());
    r.recon = reconFromJson(object.value(QStringLiteral("recon")).toObject());

    const bool valid = !r.id.isEmpty() && !r.name.isEmpty();
    if (ok) {
        *ok = valid;
    }
    return r;
}

} // namespace cbct
