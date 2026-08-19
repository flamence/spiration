/**
 * @file edit_service.h
 * @brief 编辑拓展对外暴露的服务接口。
 * @author clk
 */

#pragma once

#include <ghc/filesystem.hpp>

#include <string>

namespace spiration {
namespace edit {

/**
 * @brief 编辑拓展服务：供其它拓展将文件打开到编辑器。
 *        通过 extension_api::get_service<edit_service>() 获取。
 */
class edit_service {
public:
    virtual ~edit_service() = default;

    /// @brief 打开文件到编辑器标签页（已打开则激活对应标签页）。
    virtual void open_file(const ghc::filesystem::path& path) = 0;

    /// @brief 以预读内容打开文件（用于无法直接读盘的场景，如 OHOS 沙箱）。
    virtual void open_file_with_content(const ghc::filesystem::path& path,
                                        const std::string& content) = 0;

    /// @brief 编辑拓展的扩展 ID。
    static constexpr const char* EXTENSION_ID = "com.flamence.spiration.edit";
    /// @brief 服务名称。
    static constexpr const char* SERVICE_NAME = "edit.open";
};

} // namespace edit
} // namespace spiration
