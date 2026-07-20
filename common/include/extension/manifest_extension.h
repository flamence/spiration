/**
 * @file manifest_extension.h
 * @brief 基于 extension.json 清单的扩展基类。
 * @author clk
 */

#pragma once

#include <extension/extension.h>
#include <extension/extension_manifest.h>
#include <string>

namespace spiration {

/**
 * @brief 基于 manifest 的扩展基类。
 */
class manifest_extension : public extension {
public:
    /**
     * @brief 从 manifest JSON 文本构造。
     * @param manifest_json extension.json 的文件内容
     */
    explicit manifest_extension(const std::string& manifest_json);

    std::string id() const override          { return manifest_.id; }
    std::string name() const override        { return manifest_.name; }
    std::string version() const override     { return manifest_.version; }
    std::string description() const override { return manifest_.description; }

protected:
    manifest_data manifest_;
};

} // namespace spiration
