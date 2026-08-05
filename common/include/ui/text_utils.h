/**
 * @file text_utils.h
 * @brief 文本截断工具。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <algorithm>
#include <memory>
#include <string>

namespace spiration {
namespace text_utils {

/**
 * @brief 将 UTF-8 文本截断到可用宽度，超出部分以 "..." 结尾。
 * @return 截断后的文本；一个字符都放不下时返回空串；宽度足够时原样返回。
 */
inline std::string ellipsize(const std::shared_ptr<renderer>& r, const std::string& text,
                             float font_size, float avail_w) {
    if (text.empty() || avail_w <= 0.0f) return {};
    float tw = r ? r->measure_text_width(text, font_size) : 0.0f;
    if (tw <= avail_w) return text;

    const std::string dots = "...";
    float dots_w = r ? r->measure_text_width(dots, font_size) : 0.0f;
    float max_w = avail_w - dots_w;
    if (max_w <= 0.0f) return {};

    std::string out = text;
    while (!out.empty() && r->measure_text_width(out, font_size) > max_w) {
        out.pop_back();
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80)
            out.pop_back();
    }
    out += dots;
    return out;
}

} // namespace text_utils
} // namespace spiration
