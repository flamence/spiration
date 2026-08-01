/**
 * @file manifest_extension.cpp
 * @brief 带清单的拓展实现。
 * @author clk
 */

#include <extension/manifest_extension.h>
#include <utils/console.h>

namespace spiration {

manifest_extension::manifest_extension(const std::string& manifest_json) {
    auto result = parse_extension_manifest(manifest_json);
    if (result) {
        manifest_ = std::move(*result);
        console::info("manifest_extension", "loaded manifest for '%s' v%s",
                      manifest_.id.c_str(), manifest_.version.c_str());
    } else {
        console::error("manifest_extension", "failed to parse manifest JSON");
    }
}

} // namespace spiration
