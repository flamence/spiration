/**
 * @file diagnostics.cpp
 * @brief 宿主函数表 · 诊断（`log` / `last_error` / `reset_results`）。
 * @author clk
 */

#include "internal.h"

#include <string>

namespace spiration {
namespace capi {

namespace {

/** @brief C 日志等级 → C++ 日志等级。 */
logging::level to_log_level(spiration_log_level level) {
    switch (level) {
        case SPIRATION_LOG_TRACE: return logging::level::trace;
        case SPIRATION_LOG_DEBUG: return logging::level::debug;
        case SPIRATION_LOG_WARNING: return logging::level::warning;
        case SPIRATION_LOG_ERROR: return logging::level::error;
        case SPIRATION_LOG_INFO:
        default: return logging::level::info;
    }
}

} // namespace

void SPIRATION_CALL host_log(const spiration_extension_desc* self,
                             spiration_log_level level,
                             spiration_str_view message,
                             spiration_str_view tag) {
    std::shared_ptr<logging::logger> out = logger();
    if (!out) return;

    std::string channel = "extension/";
    channel += self ? simple_id(text_of(self->id)) : "unknown";
    const std::string tag_text = text_of(tag);
    if (!tag_text.empty()) {
        channel += "/";
        channel += tag_text;
    }

    out->log({to_log_level(level), text_of(message), channel});
}

spiration_str_view SPIRATION_CALL host_last_error(
    const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    return last_error_text();
}

void SPIRATION_CALL host_reset_results(const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    clear_results();
}

} // namespace capi
} // namespace spiration
