/**
 * @file i18n.cpp
 * @brief 宿主函数表 · 国际化（`translate` / `load_translations`）。
 * @author clk
 */

#include "internal.h"

#include <spiration/extension/builtin/i18n/i18n.h>

#include <string>
#include <vector>

namespace spiration {
namespace capi {

spiration_str_view SPIRATION_CALL host_translate(
    const spiration_extension_desc* self, spiration_str_view key,
    const spiration_str_view* args, size_t arg_count) {
    SPIRATION_UNUSED(self);
    const std::string text_key = text_of(key);
    i18n_manager& i18n = i18n_manager::get();

    if (args != NULL && arg_count > 0u) {
        std::vector<std::string> values;
        values.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) {
            values.push_back(text_of(args[i]));
        }
        /* 缺失键时回退为键本身，便于定位漏翻译的条目。 */
        return store(i18n.tr(text_key, values, text_key));
    }
    return store(i18n.tr(text_key, text_key));
}

spiration_status SPIRATION_CALL host_load_translations(
    const spiration_extension_desc* self, spiration_str_view path) {
    const std::string relative = text_of(path);
    if (relative.empty()) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "load_translations: empty path");
    }

    ghc::filesystem::path target(relative);
    if (target.is_relative()) {
        target = join_safe(extension_root(self), relative);
        if (target.empty()) {
            return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                        "load_translations: path escapes the extension directory");
        }
    }
    if (!ghc::filesystem::exists(target)) {
        return fail(SPIRATION_ERR_IO, "load_translations: file not found");
    }

    i18n_manager& i18n = i18n_manager::get();
    std::string locale = i18n.get_locale();
    if (locale.empty()) locale = "zh-CN";
    if (!i18n.load(locale, target)) {
        return fail(SPIRATION_ERR_IO, "load_translations: load failed");
    }

    if (std::shared_ptr<logging::logger> out = logger()) {
        out->log({logging::level::info,
                  "loaded translations from " + target.string(),
                  "extension/" + simple_id(text_of(self->id)) + "/i18n"});
    }
    return SPIRATION_OK;
}

} // namespace capi
} // namespace spiration
