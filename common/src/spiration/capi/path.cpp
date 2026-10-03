/**
 * @file path.cpp
 * @brief 宿主函数表 · 路径（`extension_path` / `data_path`）。
 * @author clk
 *
 * 返回的路径只做拼接与规范化，不保证目标存在。
 *
 * @note `join_safe` 与 `extension_root` 被 `i18n.cpp` 复用，故具有内部链接
 *       之外的可见性（见 `internal.h`）。
 */

#include "internal.h"

#include <spiration/application.h>
#include <spiration/extension/extension_manager.h>

#include <string>

namespace spiration {
namespace capi {

ghc::filesystem::path join_safe(const ghc::filesystem::path& base,
                                const std::string& relative) {
    if (base.empty()) return ghc::filesystem::path();

    ghc::filesystem::path joined = base;
    if (!relative.empty()) joined /= relative;
    joined = joined.lexically_normal();

    const ghc::filesystem::path root = base.lexically_normal();
    const std::string root_text = root.string();
    const std::string joined_text = joined.string();
    if (joined_text.size() < root_text.size() ||
        joined_text.compare(0, root_text.size(), root_text) != 0) {
        return ghc::filesystem::path();
    }
    return joined;
}

ghc::filesystem::path extension_root(const spiration_extension_desc* self) {
    extension_manager* mgr = manager();
    if (mgr == nullptr || self == nullptr) return ghc::filesystem::path();
    return mgr->extension_directory(text_of(self->id));
}

spiration_str_view SPIRATION_CALL host_extension_path(
    const spiration_extension_desc* self, spiration_str_view relative) {
    const ghc::filesystem::path path =
        join_safe(extension_root(self), text_of(relative));
    if (path.empty()) {
        set_last_error("extension_path: path escapes the extension directory");
        return empty_view();
    }
    return store(path.string());
}

spiration_str_view SPIRATION_CALL host_data_path(
    const spiration_extension_desc* self, spiration_str_view relative) {
    application* app = application::instance();
    const ghc::filesystem::path base =
        (app == nullptr || self == nullptr)
            ? ghc::filesystem::path()
            : app->data_directory(text_of(self->id));

    const ghc::filesystem::path path = join_safe(base, text_of(relative));
    if (path.empty()) {
        set_last_error("data_path: path escapes the data directory");
        return empty_view();
    }
    return store(path.string());
}

} // namespace capi
} // namespace spiration
