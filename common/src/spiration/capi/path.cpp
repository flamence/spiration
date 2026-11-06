/**
 * @file path.cpp
 * @brief 宿主函数表 · 路径（`extension_path` / `data_path`）。
 * @author clk
 *
 * 两个函数都以拓展自身的文件夹为根目录：`extension_path` 用于只读的安装
 * 文件，`data_path` 用于可读写的私有数据。拓展文件夹由
 * `extension_manager::extension_directory` 给出，即装载时扫描拓展的目录下
 * 以拓展 ID 命名的子目录（例如 `<数据目录>/com.example.hello-world`）；
 * 不再把私有数据放进应用包名对应的子目录里。
 *
 * 返回的路径只做拼接与规范化，不保证目标存在。
 *
 * @note `join_safe` 与 `extension_root` 被 `i18n.cpp` 复用，故具有内部链接
 *       之外的可见性（见 `internal.h`）。
 */

#include "internal.h"

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
    /* 拓展文件夹即拓展的私有数据目录：装载时拓展从其自身文件夹读取文件，
       该文件夹位于数据目录下、可读写且随版本保留，无需再嵌套一层应用包名。 */
    const ghc::filesystem::path path =
        join_safe(extension_root(self), text_of(relative));
    if (path.empty()) {
        set_last_error("data_path: path escapes the data directory");
        return empty_view();
    }
    return store(path.string());
}

} // namespace capi
} // namespace spiration
