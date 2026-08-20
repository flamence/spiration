/**
 * @file ime.h
 * @brief 输入法光标位置上报定义。
 * @author clk
 */

#pragma once

#include <functional>

namespace spiration {
namespace ime {

/**
 * @brief 注册输入法组合窗口位置回调。
 * @param cb 接收窗口坐标的回调
 */
void set_caret_callback(std::function<void(float, float)> cb);

/**
 * @brief 报告光标窗口坐标，平台据此移动输入法组合窗口。
 * @param x 光标窗口横坐标
 * @param y 光标窗口纵坐标
 */
void set_caret_rect(float x, float y);

} // namespace ime
} // namespace spiration
