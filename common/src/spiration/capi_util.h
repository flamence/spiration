/**
 * @file capi_util.h
 * @brief C API 适配层的内部工具（宿主私有）。
 * @author clk
 */

#pragma once

#include <spiration/api.h>

#include <string>

namespace spiration {
namespace capi {
namespace detail {

/** @brief 字符串视图 → `std::string`。 */
inline std::string text_of(spiration_str_view text) {
    return text.data ? std::string(text.data, text.length) : std::string();
}

/** @brief `std::string` → 字符串视图（借用，调用方保证生命周期）。 */
inline spiration_str_view view_of(const std::string& text) {
    spiration_str_view out;
    out.data = text.empty() ? NULL : text.data();
    out.length = text.size();
    return out;
}

/** @brief 空字符串视图。 */
inline spiration_str_view empty_view() {
    spiration_str_view out;
    out.data = NULL;
    out.length = 0u;
    return out;
}

/** @brief 取拓展短名（`com.example.hello-world` → `hello-world`）。 */
inline std::string simple_id(const std::string& id) {
    const size_t pos = id.find_last_of('.');
    return pos == std::string::npos ? id : id.substr(pos + 1);
}

} // namespace detail
} // namespace capi
} // namespace spiration
