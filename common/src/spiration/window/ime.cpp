/**
 * @file ime.cpp
 * @brief 输入法光标位置上报实现。
 * @author clk
 */

#include <spiration/window/ime.h>

namespace spiration {
namespace ime {

namespace {

std::function<void(float, float)> caret_callback_;

} // namespace

void set_caret_callback(std::function<void(float, float)> cb) {
    caret_callback_ = std::move(cb);
}

void set_caret_rect(float x, float y) {
    if (caret_callback_) caret_callback_(x, y);
}

} // namespace ime
} // namespace spiration
