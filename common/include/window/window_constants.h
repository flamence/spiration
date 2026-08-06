/**
 * @file window_constants.h
 * @brief 跨平台窗口常量（自绘标题栏 / 边框 / 最小尺寸）。
 * @author clk
 */

#pragma once

namespace spiration {

/// 自绘标题栏拖拽区高度（DIP）。
inline constexpr float kTitleBarDragHeight = 34.0f;

/// 窗口边缘可调整大小的边框宽度（DIP）。
inline constexpr float kResizeBorderWidth = 6.0f;

/// 窗口最小宽度（DIP）。
inline constexpr float kMinWindowWidth = 400.0f;

/// 窗口最小高度（DIP）。
inline constexpr float kMinWindowHeight = 300.0f;

} // namespace spiration
