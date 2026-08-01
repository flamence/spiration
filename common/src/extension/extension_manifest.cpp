/**
 * @file extension_manifest.cpp
 * @brief extension.json 解析器实现。
 * @author clk
 */

#include <extension/extension_manifest.h>

namespace spiration {

std::optional<manifest_data> parse_extension_manifest(const std::string& json) {
    try {
        nlohmann::json root = nlohmann::json::parse(json);
        if (!root.is_object()) {
            console::error("extension_manifest", "root is not a json object");
            return {};
        }

        manifest_data m;
        m.id          = root.value("id", std::string());
        m.name        = root.value("name", std::string());
        m.version     = root.value("version", std::string());
        m.description = root.value("description", std::string());
        m.author      = root.value("author", std::string());
        m.main        = root.value("main", std::string());
        m.api_version = root.value("api_version", 0);

        auto dep_it = root.find("depends");
        if (dep_it != root.end() && dep_it->is_object()) {
            for (auto& [k, v] : dep_it->items()) {
                if (v.is_string()) {
                    m.depends[k] = v.get<std::string>();
                }
            }
        }

        if (m.id.empty() || m.name.empty() || m.version.empty() || m.main.empty()) {
            console::error("extension_manifest", "missing required fields (id/name/version/main)");
            return {};
        }

        return m;
    } catch (const nlohmann::json::exception& e) {
        console::error("extension_manifest", "invalid extension.json: %s", e.what());
        return {};
    }
}

} // namespace spiration
