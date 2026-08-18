/**
 * @file properties.h
 * @author clk
 * @brief 属性工具定义。
 */

#pragma once

#include <ghc/filesystem.hpp>
#include <tl/optional.hpp>
#include <string>
#include <unordered_map>

namespace spiration {

/**
 * @brief 属性类。
 */
class properties {
public:
    properties() = default;

    /**
     * @brief 从文件加载并解析。
     * @param filepath 文件路径
     * @return 文件是否存在且成功解析。
     */
    bool load(const ghc::filesystem::path& filepath);

    /**
     * @brief 从内存文本解析。
     * @param text 格式化文本
     */
    void parse(const std::string& text);

    /**
     * @param key 键
     * @return 是否包含指定键。
     */
    bool contains(const std::string& key) const;

    /**
     * @param key 键
     * @return 指定键的值。
     */
    tl::optional<std::string> get(const std::string& key) const;

    /**
     * @return 全部键值对。
     */
    const std::unordered_map<std::string, std::string>& entries() const;

    /**
     * @return 键值对数量。
     */
    std::size_t size() const;

private:
    std::unordered_map<std::string, std::string> data_;
};

} // namespace spiration
