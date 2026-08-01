/**
 * @file edit_tool.h
 * @brief 文件编辑工具集。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>
#include <string>

namespace spiration {
namespace agent {

/// @brief 创建文件。
class create_file_tool : public tool {
public:
    std::string name() const override { return "create_file"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

/// @brief 创建目录。
class create_directory_tool : public tool {
public:
    std::string name() const override { return "create_directory"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

/// @brief 编辑文件。
class edit_file_tool : public tool {
public:
    std::string name() const override { return "edit_file"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

/// @brief 重命名/移动文件或目录。
class rename_tool : public tool {
public:
    std::string name() const override { return "rename"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

/// @brief 删除文件或目录。
class delete_tool : public tool {
public:
    std::string name() const override { return "delete"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

} // namespace agent
} // namespace spiration
