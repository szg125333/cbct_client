/**
 * @file recipe_params.h
 * @brief 配方参数结构体定义（纯数据，不含业务逻辑）
 *
 * 配方按硬件/算法拆成子模块，再汇总到 Recipe：
 *   HvgParams      高压发生器（kV/mA/曝光时间…）
 *   DetectorParams 平板探测器（分辨率/帧率/增益…）
 *   GantryParams   龙门架旋转（转速/角度/帧数…）
 *   ReconParams    重建算法（FDK 等、体素、GPU…）
 */
#ifndef CBCT_RECIPE_PARAMS_H
#define CBCT_RECIPE_PARAMS_H

#include <QString>
#include <QMetaType>

namespace cbct {

/// @brief 高压发生器参数：决定射线质与剂量
struct HvgParams {
    double tubeVoltageKv = 80.0;    ///< 管电压 (kV)：影响射线穿透力
    double tubeCurrentMa = 20.0;    ///< 管电流 (mA)：影响射线强度/剂量
    int exposureTimeMs = 500;       ///< 曝光时间 (ms)：单帧或总曝光时长
    int pulseCount = 1;             ///< 脉冲次数：多次短脉冲曝光时用
    double filamentPreheat = 0.0;   ///< 灯丝预热参数（预留，真机再定义单位）
};

/// @brief X 射线平板探测器（FPD）参数：决定采图几何与数据格式
struct DetectorParams {
    int binning = 1;                ///< 像素合并倍数（1=不合并，2=2x2 合并）
    double pixelPitchMm = 0.2;      ///< 像素间距 (mm)
    int width = 1024;               ///< 图像宽 (px)
    int height = 1024;              ///< 图像高 (px)
    int frameRateFps = 30;          ///< 采集帧率 (fps)：模拟器用它算定时器间隔
    double gain = 1.0;              ///< 增益（标定用，模拟阶段仅记录）
    double offset = 0.0;            ///< 偏置/暗场偏移（标定用）
};

/// @brief 龙门架 / 旋转扫描参数：控制 C 臂或机架如何转一圈
struct GantryParams {
    double rotationSpeedDegS = 20.0; ///< 旋转速度 (°/s)
    double startAngleDeg = 0.0;      ///< 起始角 (°)
    double endAngleDeg = 360.0;      ///< 终止角 (°)：通常 360° 全周
    int totalFrames = 600;           ///< 计划投影帧数（真机按角步进算）
    double pitchMmPerRot = 0.0;      ///< 螺距 (mm/圈)；轴向扫描用，圆周扫描可为 0
};

/**
 * @brief 三维重建（Recon）参数：投影采完后体数据如何重建
 *
 * 当前仅在配方对话框「重建算法」页编辑并写入 recipes.json；
 * 真正重建管线尚未接入 core，字段先按常见 CBCT/FDK 约定预留。
 * 对应 UI：RecipeDialog::tabRecon（非独立 recon_panel 类）。
 */
struct ReconParams {
    /// 算法名：FDK（滤波反投影，锥束常用）/ SART / SIRT（迭代类）
    QString algorithm = QStringLiteral("FDK");
    /// 输出体素边长 (mm)：越小越细、内存越大
    double voxelSizeMm = 0.3;
    /// 迭代次数：仅 SART/SIRT 等有效；FDK 单次反投影可忽略
    int iterations = 20;
    /// 滤波核名称（FDK 前后滤波）：如 standard / soft / sharp（按实现定义）
    QString filterKernel = QStringLiteral("standard");
    /// 是否走 GPU 加速；模拟/无 GPU 时可关掉走 CPU
    bool gpuEnabled = true;
};

/// @brief 一份完整配方：UI 选中后下发给采集/重建流程
struct Recipe {
    QString id;          ///< UUID 字符串，唯一标识，JSON 持久化用
    QString name;        ///< 显示名，如下拉框中的「成人头部 CBCT」
    QString description; ///< 描述，可选
    HvgParams hvg;               ///< 高压参数组
    DetectorParams detector;     ///< 探测器参数组
    GantryParams gantry;         ///< 龙门架参数组
    ReconParams recon;           ///< 重建参数组
};

} // namespace cbct

// 供 Qt 元对象系统在跨线程 Q_ARG 中识别该类型
Q_DECLARE_METATYPE(cbct::DetectorParams)

#endif // CBCT_RECIPE_PARAMS_H
