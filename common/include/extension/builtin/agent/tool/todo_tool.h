/**
 * @file todo_tool.h
 * @brief 待办事项工具。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/// @brief 待办事项状态。
enum class todo_status {
    pending,     ///< 待处理
    in_progress, ///< 进行中
    completed,   ///< 已完成
};

/// @brief 待办事项。
struct todo_item {
    std::string id;
    std::string content;
    todo_status status = todo_status::pending;
};

/// @brief 将状态字符串解析为枚举。
todo_status todo_status_from_string(const std::string& s);

/// @brief 将枚举转换为状态字符串。
std::string todo_status_to_string(todo_status s);

/**
 * @brief 线程安全的待办列表存储。
 */
class todo_store {
public:
    static todo_store& instance();

    /// @brief 获取当前会话的待办列表。
    std::vector<todo_item> items() const;

    /// @brief 变更版本号（原子读取，供 UI 线程轮询）。
    uint64_t version() const {
        return version_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 用全量列表替换当前会话的列表。
     * @param incoming 模型下发的列表；按 content 匹配以保持既有 id 稳定。
     */
    void set(const std::vector<todo_item>& incoming);

    /// @brief 清空当前会话的待办列表。
    void clear();

    /// @brief 切换当前会话。
    void set_current_uuid(const std::string& uuid);

    /// @brief 删除某会话的待办列表。
    void remove(const std::string& uuid);

private:
    todo_store() = default;
    mutable std::mutex mtx_;
    std::map<std::string, std::vector<todo_item>> lists_;
    std::string current_;
    std::atomic<uint64_t> version_{0};
    size_t next_id_ = 1;
};

/// @brief 待办事项工具。
class todo_tool : public tool {
public:
    std::string name() const override { return "todo"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

} // namespace agent
} // namespace spiration
