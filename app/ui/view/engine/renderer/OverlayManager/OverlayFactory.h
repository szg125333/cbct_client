#pragma once

#include <memory>

class IOverlayManager;

/**
 * @brief Overlay 管理器工厂
 *
 * 集中管理 Overlay Feature 的组合，避免创建逻辑散落各处。
 * 目前提供一个标准配置，后续可按需扩展（如不含测角的精简配置）。
 */
class OverlayFactory {
public:
    /**
     * @brief 创建包含完整功能集的 Overlay 管理器
     *
     * 默认包含：
     *   - SimpleCrosshairManager    （十字线定位）
     *   - SimpleDistanceMeasureManager（距离测量标注）
     *   - SimpleAngleMeasureManager （角度测量标注）
     *   - SimpleOverlayInfoManager  （窗宽窗位 / 切片信息文字）
     */
    static std::unique_ptr<IOverlayManager> CreateDefault();
};
