/**
 * @file chat_store.h
 * @brief 对话记录存取。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/chat.h>
#include <extension/builtin/agent/tool/terminal_tool.h>
#include <extension/builtin/agent/tool/todo_tool.h>

#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/// @brief 会话元信息。
struct conversation_meta {
    std::string uuid;
    std::string name;
    std::string created_at;
    std::string updated_at;
    std::string provider;
    std::string model;
    size_t message_count = 0;
};

/// @brief 一次对话存档。
struct chat_archive {
    std::string uuid;
    std::string name;
    std::string created_at;
    std::string provider;
    std::string model;
    std::string memory;
    std::vector<chat_message> messages;
    std::vector<terminal_snapshot> terminals;
    std::vector<todo_item> todos;
    bool can_continue = false;
    bool auto_approve = false;
    long long tokens_in = 0;
    long long tokens_out = 0;
};

/**
 * @brief 对话记录存取。
 */
class chat_store {
public:
    /**
     * @brief 构造存储。
     * @param base_dir 数据目录。
     */
    explicit chat_store(std::string base_dir);

    /**
     * @brief 创建新会话。
     * @param name 会话名，空则自动生成。
     * @return 会话 UUID，失败返回空串。
     */
    std::string create(const std::string& name);

    /// @brief 保存会话，成功返回 true。
    bool save(const chat_archive& archive);

    /// @brief 加载会话，成功返回 true。
    bool load(const std::string& uuid, chat_archive& archive) const;

    /// @brief 删除会话目录，成功返回 true。
    bool remove(const std::string& uuid);

    /// @brief 列出全部会话（按 updated_at 倒序）。
    std::vector<conversation_meta> list() const;

    /// @brief 读取会话的 memory.md。
    std::string load_memory(const std::string& uuid) const;

    /// @brief 写入会话的 memory.md。
    bool save_memory(const std::string& uuid, const std::string& content);

    /// @brief 当前会话 UUID。
    std::string current_uuid() const;
    void set_current_uuid(const std::string& uuid);

    /// @brief 数据目录。
    const std::string& base_dir() const { return base_dir_; }

    /// @brief chat/ 子目录路径。
    std::string chat_dir() const;

    /// @brief 会话目录路径。
    std::string conversation_dir(const std::string& uuid) const;

private:
    std::string base_dir_;
    mutable std::mutex uuid_mtx_;
    std::string current_uuid_;

    /// @brief 生成 UUID v4 字符串。
    static std::string make_uuid();
};

} // namespace agent
} // namespace spiration
