/**
 * @file extension_manifest.h
 * @brief 拓展清单数据结构与解析。
 * @author clk
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

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
 * @brief 解析 extension.json 文本为 manifest_data。
 * @param json extension.json 的 UTF-8 文本内容
 * @return 成功返回 manifest_data，失败返回 nullopt
 */
std::optional<manifest_data> parse_extension_manifest(const std::string& json);

} // namespace spiration
