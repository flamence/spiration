/**
 * @file manifest_extension.cpp
 * @brief 带清单的拓展实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/manifest_extension.h>

#include <format>

namespace spiration {

manifest_extension::manifest_extension(const std::string& manifest_json) {
    auto result = parse_extension_manifest(manifest_json);
    if (result) {
        manifest_ = std::move(*result);
        application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("loaded manifest for '{}' v{}", manifest_.id, manifest_.version), .tag = "manifest_extension" });
    } else {
        application::instance()->logger()->log({ .level = logging::level::error, .message = "failed to parse manifest JSON", .tag = "manifest_extension" });
    }
}

} // namespace spiration
