/**
 * @file memory_tool.h
 * @brief 记忆工具。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/chat_store.h>
#include <extension/builtin/agent/tool/tool.h>

#include <functional>
#include <string>

namespace spiration {
namespace agent {

/**
 * @brief 记忆工具。
 */
class memory_tool : public tool {
public:
    std::string name() const override { return "memory"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;

    /// @brief 绑定存储与当前会话 UUID 获取器。
    void bind(chat_store* store, std::function<std::string()> current_uuid) {
        store_ = store;
        current_uuid_ = std::move(current_uuid);
    }

private:
    chat_store* store_ = nullptr;
    std::function<std::string()> current_uuid_;
};

} // namespace agent
} // namespace spiration
