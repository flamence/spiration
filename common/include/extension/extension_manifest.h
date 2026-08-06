/**
 * @file extension_manifest.h
 * @brief 拓展清单数据结构与解析。
 * @author clk
 */

#pragma once

#include <utils/console.h>

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <optional>

#include <nlohmann/json.hpp>

namespace spiration {

/**
 * @brief extension.json 解析后的结构化数据。
 */
struct manifest_data {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    int api_version = 0;
    std::string main;
    std::map<std::string, std::string> depends;
};

/**
 * @brief 当前支持的扩展 API 版本。
 * @note 外部扩展 api_version 为 0（未声明）视为兼容；声明了非本值的版本将被拒绝加载。
 */
inline constexpr int kSupportedExtensionApiVersion = 1;

/**
 * @brief 语义化版本比较。
 * @return a < b 返回 -1；a == b 返回 0；a > b 返回 1。
 */
int compare_versions(const std::string& a, const std::string& b);

/**
 * @brief 检查 version 是否满足 constraint。
 * @param constraint 支持 =/==/!=/</<=/>/>= 与 ~（波浪号）、^（兼容）前缀；空或 * 表示任意版本。
 */
bool version_matches(const std::string& version, const std::string& constraint);

/**
 * @brief 解析 extension.json 数据为 manifest_data。
 * @param json extension.json 内容
 * @return 成功返回 `manifest_data`，失败返回 `nullopt`
 */
std::optional<manifest_data> parse_extension_manifest(const std::string& json);

} // namespace spiration
