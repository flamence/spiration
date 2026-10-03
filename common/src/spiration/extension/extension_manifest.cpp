/**
 * @file extension_manifest.cpp
 * @brief extension.json 解析器实现。
 * @author clk
 */

#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <spiration/application.h>
#include <spiration/extension/extension_manifest.h>
#include <nlohmann/json.hpp>

namespace spiration {

namespace {

std::vector<int> version_parts(const std::string& s) {
    std::vector<int> parts;
    std::string cur;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            cur += c;
        } else if (c == '.') {
            if (!cur.empty()) {
                parts.push_back(std::atoi(cur.c_str()));
                cur.clear();
            }
        } else {
            break;
        }
    }
    if (!cur.empty()) parts.push_back(std::atoi(cur.c_str()));
    return parts;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string upper_bound(const std::string& ver, bool tilde) {
    auto parts = version_parts(ver);
    if (parts.empty()) return "999999";
    if (tilde) {
        if (parts.size() >= 2) {
            return std::to_string(parts[0]) + "." + std::to_string(parts[1] + 1);
        }
        return std::to_string(parts[0] + 1);
    }
    if (parts[0] > 0) return std::to_string(parts[0] + 1);
    if (parts.size() >= 2) return "0." + std::to_string(parts[1] + 1);
    return "0.0." + std::to_string(parts.size() >= 3 ? parts[2] + 1 : 1);
}

} // namespace

int compare_versions(const std::string& a, const std::string& b) {
    auto pa = version_parts(a);
    auto pb = version_parts(b);
    size_t n = std::max(pa.size(), pb.size());
    for (size_t i = 0; i < n; ++i) {
        int va = i < pa.size() ? pa[i] : 0;
        int vb = i < pb.size() ? pb[i] : 0;
        if (va < vb) return -1;
        if (va > vb) return 1;
    }
    return 0;
}

bool version_matches(const std::string& version, const std::string& constraint) {
    std::string c = trim(constraint);
    if (c.empty() || c == "*") return true;

    std::string op;
    std::string ver;
    for (const char* pre : {">=", "<=", "==", "!=", ">", "<", "=", "~", "^"}) {
        if (c.rfind(pre, 0) == 0) {
            op = pre;
            ver = trim(c.substr(std::strlen(pre)));
            break;
        }
    }
    if (op.empty()) {
        op = "=";
        ver = c;
    }

    int cmp = compare_versions(version, ver);
    if (op == "!=") return cmp != 0;
    if (op == ">")  return cmp > 0;
    if (op == "<")  return cmp < 0;
    if (op == ">=") return cmp >= 0;
    if (op == "<=") return cmp <= 0;
    if (op == "~" || op == "^") {
        return cmp >= 0 &&
               compare_versions(version, upper_bound(ver, op == "~")) < 0;
    }
    return cmp == 0;
}

tl::optional<manifest_data> parse_extension_manifest(const std::string& json) {
    try {
        nlohmann::json root = nlohmann::json::parse(json);
        if (!root.is_object()) {
            application::instance()->logger()->log({logging::level::error, "root is not a json object", "extension_manifest"});
            return {};
        }

        manifest_data m;
        m.id          = root.value("id", std::string());
        m.version     = root.value("version", std::string());
        m.api_version = root.value("api_version", 0);

        std::string author = root.value("author", std::string());
        if (!author.empty()) m.author = author;

        std::string main = root.value("main", std::string());
        if (!main.empty()) m.main = main;

        std::string loader = root.value("loader", std::string());
        if (!loader.empty()) m.loader = loader;

        auto dep_it = root.find("depends");
        if (dep_it != root.end() && dep_it->is_object()) {
            for (auto& item : dep_it->items()) {
                const auto& k = item.key();
                auto& v = item.value();
                if (v.is_string()) {
                    m.depends[k] = v.get<std::string>();
                }
            }
        }

        if (m.id.empty() || m.version.empty() ||
            !m.main.has_value() || m.main->empty()) {
            application::instance()->logger()->log({logging::level::error, "missing required fields (id/version/main)", "extension_manifest"});
            return {};
        }

        return m;
    } catch (const nlohmann::json::exception& e) {
        application::instance()->logger()->log({logging::level::debug, fmt::format("invalid extension.json: {}", e.what()), "extension_manifest"});
        return {};
    }
}

} // namespace spiration
