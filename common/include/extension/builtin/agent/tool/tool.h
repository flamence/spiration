/**
 * @file tool.h
 * @brief 智能体工具基类接口。
 * @author clk
 */

#pragma once

#include <utils/path.h>

#include <functional>
#include <string>

namespace spiration {
namespace agent {

/// @brief 工具定义。
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
    void set_workdir(std::function<std::string()> getter) {
        workdir_getter_ = std::move(getter);
    }

    /**
     * @brief 解析路径。
     */
    std::string resolve_path(const std::string& raw) const {
        if (raw.empty()) return raw;
        if (path::u8path(raw).is_absolute()) return raw;
        if (!workdir_getter_) return raw;
        std::string wd = workdir_getter_();
        if (wd.empty()) return raw;
        if (wd.back() != '/' && wd.back() != '\\') wd += '/';
        return wd + raw;
    }

    /**
     * @brief 转换为 tool_definition 供 chat_client 注册。
     */
    tool_definition to_definition() const {
        return {name(), description(), parameters_json()};
    }

protected:
    std::function<std::string()> workdir_getter_;
};

} // namespace agent
} // namespace spiration
