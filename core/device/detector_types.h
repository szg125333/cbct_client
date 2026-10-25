/**
 * @file detector_types.h
 * @brief 平板采集状态机枚举
 */
#ifndef CBCT_DETECTOR_TYPES_H
#define CBCT_DETECTOR_TYPES_H

namespace cbct {

/**
 * @enum DetectorState
 * @brief 采集器生命周期状态
 *
 * Idle         空闲，可配置
 * ApplyingParams 正在校验/记录平板参数
 * Initializing 打开投影序列、分配缓冲
 * Running      定时出帧中
 * Finished     已推完全部帧
 * Error        失败（原因见 DetectorSimulator::lastError）
 */
enum class DetectorState {
    Idle,
    ApplyingParams,
    Initializing,
    Running,
    Finished,
    Error
};

} // namespace cbct

#endif // CBCT_DETECTOR_TYPES_H
