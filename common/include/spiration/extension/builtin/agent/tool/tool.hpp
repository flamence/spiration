/**
 * @file tool.hpp
 * @brief 智能体工具基类接口（含内联实现）。
 * @author clk
 */

#pragma once

#include <ghc/filesystem.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace spiration {
namespace agent {

/// @brief 工具定义。
struct tool_definition {
    std::string function_name;
    std::string description;
    /// @brief 参数 JSON Schema 的结构化对象（web_provider 处理为 JSON Schema 发送）。
    nlohmann::json parameters;
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

    /// @brief 参数 JSON Schema 的结构化对象。
    virtual nlohmann::json parameters() const = 0;

    /**
     * @brief 执行工具。
     * @param args_json  JSON 格式的参数字符串
     * @return 工具执行结果
     */
    virtual std::string execute(const std::string& args_json) = 0;

    /**
     * @brief 默认超时秒数。
     */
    virtual long default_timeout_seconds() const { return 30; }

    /**
     * @brief 是否串行执行。
     */
    virtual bool serial() const { return false; }

    /**
     * @brief 是否需要用户批准后才执行。
     */
    virtual bool requires_approval() const { return false; }

    /**
     * @brief 取消回调。
     */
    std::function<bool()> should_stop;

    /**
     * @brief 设置工作目录获取器。
     *        未设置时相对路径保持原样。
     */
    void set_workdir(std::function<ghc::filesystem::path()> getter) {
        workdir_getter_ = std::move(getter);
    }

    /**
     * @brief 转换为 tool_definition 供 chat_client 注册。
     */
    tool_definition to_definition() const {
        return {name(), description(), parameters()};
    }

protected:
    std::function<ghc::filesystem::path()> workdir_getter_;
};

} // namespace agent
} // namespace spiration
