/**
 * @file tool.h
 * @brief 智能体工具基类接口。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {
namespace agent {

/// @brief 工具定义，用于 function calling 注册。
struct tool_definition {
    std::string function_name;
    std::string description;
    std::string parameters_json;
};

/**
 * @brief 工具抽象基类。
 */
class tool {
public:
    virtual ~tool() = default;

    /// @brief 工具名称。
    virtual std::string name() const = 0;

    /// @brief 工具描述。
    virtual std::string description() const = 0;

    /// @brief 参数 JSON Schema 字符串。
    virtual std::string parameters_json() const = 0;

    /**
     * @brief 执行工具。
     * @param args_json  JSON 格式的参数字符串
     * @return 工具执行结果
     */
    virtual std::string execute(const std::string& args_json) = 0;

    /**
     * @brief 转换为 tool_definition 供 chat_client 注册。
     */
    tool_definition to_definition() const {
        return {name(), description(), parameters_json()};
    }
};

} // namespace agent
} // namespace spiration
