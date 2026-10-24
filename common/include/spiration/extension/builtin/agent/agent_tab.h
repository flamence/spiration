/**
 * @file agent_tab.h
 * @brief 智能体标签页控件（一个标签页即一个对话）。
 * @author clk
 */

#pragma once

#include <spiration/extension/builtin/agent/chat.h>
#include <spiration/extension/builtin/agent/chat_store.h>
#include <spiration/extension/builtin/agent/registry.h>
#include <spiration/extension/builtin/agent/ui/todo_view.h>

#include <spiration/ui/button.h>
#include <spiration/ui/checkbox.h>
#include <spiration/ui/collapsible.h>
#include <spiration/ui/combo_box.h>
#include <spiration/ui/container.h>
#include <spiration/ui/input_dialog.h>
#include <spiration/ui/label.h>
#include <spiration/ui/layout.h>
#include <spiration/ui/markdown.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/ui/text_area.h>
#include <spiration/ui/theme_manager.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

struct display_message {
    std::string role;
    std::string content;
};

/// @brief 后台加载会话存档的结果。
struct initial_load_result {
    chat_archive archive;
    bool loaded = false;
};

/**
 * @brief 智能体标签页：一个标签页承载一个对话，不再内嵌会话列表。
 *
 * 会话切换由标签栏（+ / 历史菜单 / 右键菜单）完成，本控件只专注聊天区。
 */
class agent_tab : public tab {
public:
    agent_tab(std::shared_ptr<chat_client> client,
              std::shared_ptr<chat_store> store,
              std::string conversation_uuid,
              std::string conversation_name = {});
    ~agent_tab() override;

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void layout() override;
    void tick(float dt_ms) override;
    void on_activate() override;
    widget* hit_test_hover(float x, float y) const override;

    /// @brief 接管客户端已注册工具的所有权（客户端只持有裸指针）。
    void set_tools(std::vector<std::unique_ptr<tool>> tools) { tools_ = std::move(tools); }

    /// @brief 设置可选模型列表。
    void set_models(std::vector<model_option> models);

    /// @brief 模型/思考挡位变更回调，供拓展记住下次新建标签的默认配置。
    std::function<void(const chat_client::config&)> on_config_changed;

    /// @brief 标签页被关闭时回调，供持有者清理自身对该标签页的引用。
    std::function<void()> on_destroyed;

    /// @brief 删除对话后请求关闭本标签页。
    std::function<void(agent_tab*)> on_delete_requested;

    const std::string& conversation_uuid() const { return conversation_uuid_; }
    const std::string& conversation_name() const { return name_; }

    /// @brief 等待后台初始加载完成（释放存储前调用）。
    void wait_initial_load();

    /// @brief 同步保存当前对话（关闭标签页/退出应用时使用）。
    void save_now();

    /// @brief 重命名对话（弹出输入框）。
    void begin_rename();

    /// @brief 请求删除当前对话（输入对话名确认后删除）。
    void begin_delete();

    /// @brief 删除当前对话（由拓展在确认后调用）。
    void delete_conversation();

    /// @brief 是否可以继续生成。
    bool can_continue() const { return continue_btn_ != nullptr; }

    /// @brief 当前是否自动审批敏感工具。
    bool auto_approve() const { return auto_approve_on_.load(); }

private:
    // 共享所有权：后台线程在拓展关闭后仍可安全使用。
    std::shared_ptr<chat_store> store_;
    std::vector<std::unique_ptr<tool>> tools_;
    std::shared_ptr<chat_client> client_;

    std::string conversation_uuid_;
    std::string name_;
    std::string created_at_;
    bool auto_title_ = false;
    /// @brief 对话已被删除：析构时不得再次写盘。
    bool deleted_ = false;

    container* scroll_ = nullptr;
    container* settings_bar_ = nullptr;
    container* input_bar_ = nullptr;
    container* msg_list_ = nullptr;
    todo_view* todo_view_ = nullptr;

    combo_box* model_combo_ = nullptr;
    combo_box* reasoning_combo_ = nullptr;
    checkbox* auto_approve_ = nullptr;
    label* token_label_ = nullptr;

    collapsible* todo_capsule_ = nullptr;
    size_t todo_count_ = 0;
    bool todo_has_ = false;

    text_area* input_ = nullptr;
    button* send_btn_ = nullptr;

    static constexpr float MAX_INPUT_H = 120.0f;
    void update_input_height();

    std::vector<display_message> messages_;
    bool layout_batch_ = false;
    bool scroll_bottom_pending_ = false;
    /// @brief 激活后等布局完成再聚焦输入框（此时光标位置才有意义）。
    bool focus_input_pending_ = false;

    reasoning_level reasoning_ = reasoning_level::medium;

    collapsible* tool_capsule_ = nullptr;
    container* tool_capsule_scroll_ = nullptr;
    markdown* tool_label_ = nullptr;
    collapsible* thinking_capsule_ = nullptr;
    container* thinking_scroll_ = nullptr;
    markdown* thinking_label_ = nullptr;

    std::future<chat_response> pending_;
    std::future<initial_load_result> initial_load_;
    bool waiting_ = false;

    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<std::atomic<bool>> stop_flag_ = std::make_shared<std::atomic<bool>>(false);
    std::atomic<bool> supplement_requested_{false};
    std::deque<std::string> queued_inputs_;
    button* continue_btn_ = nullptr;
    std::atomic<bool> tokens_dirty_{false};
    std::vector<model_option> models_;

    struct stream_event {
        enum class type { delta, reasoning, tool_call, tool_result };
        type t = type::delta;
        std::string text;
        std::string name;
        std::string args;
    };
    std::mutex stream_mutex_;
    std::deque<stream_event> stream_events_;
    markdown* stream_label_ = nullptr;
    markdown* last_label_ = nullptr;
    size_t stream_msg_index_ = 0;
    bool streamed_content_ = false;
    std::string utf8_hold_reasoning_;
    std::string utf8_hold_delta_;

    std::mutex approval_mtx_;
    std::atomic<bool> approval_pending_{false};
    tool_call approval_tc_;
    std::shared_ptr<std::promise<bool>> approval_promise_;
    std::atomic<bool> auto_approve_on_{false};
    bool approval_inline_shown_ = false;
    label* approval_hint_ = nullptr;
    container* approval_row_ = nullptr;
    button* approval_allow_btn_ = nullptr;
    button* approval_deny_btn_ = nullptr;

    input_dialog* rename_dialog_ = nullptr;

    /** @brief 只标脏并请求重绘；布局由帧循环合并执行。 */
    void schedule_layout();
    /** @brief 下一帧滚动到底部（内容增长后调用）。 */
    void schedule_scroll_bottom();

    void apply_initial_load(const initial_load_result& res);
    void clear_messages();
    void save_async();
    void build_archive(chat_archive& archive) const;
    /** @brief 根据首条用户消息生成标签标题。 */
    void auto_name_from(const std::string& text);

    void show_inline_approval();
    void remove_inline_approval();

    /** @brief 追加一条消息（角色 + 内容）。 */
    void add_message(const std::string& role, const std::string& content);
    markdown* append_bubble(const display_message& msg);
    void process_stream_events(size_t max_events = 0);
    void send();
    void start_send(const std::string& text, bool show_user);
    void begin_run();
    void continue_generation();
    void add_continue_button();
    void remove_continue_button();
    void update_send_button();
    void update_token_label();

    void set_reasoning_level(reasoning_level l);
    void refresh_reasoning_combo();
    void update_todo_capsule();
    collapsible* create_capsule(const std::string& summary);
    collapsible* append_tool_capsule(const std::string& summary, const std::string& args);
    void append_tool_result(const std::string& text);
    void restore_history_ui(const std::vector<chat_message>& msgs);

    bool approve_request(const tool_call& tc);
    void resolve_approval(bool ok);
};

} // namespace agent
} // namespace spiration
