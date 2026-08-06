/**
 * @file agent_tab.h
 * @brief 智能体标签页控件。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/chat.h>
#include <extension/builtin/agent/chat_store.h>
#include <extension/builtin/agent/ui/todo_view.h>

#include <ui/button.h>
#include <ui/checkbox.h>
#include <ui/collapsible.h>
#include <ui/combo_box.h>
#include <ui/container.h>
#include <ui/input_dialog.h>
#include <ui/label.h>
#include <ui/layout.h>
#include <ui/markdown.h>
#include <ui/rectangle.h>
#include <ui/split_pane.h>
#include <ui/tab_bar.h>
#include <ui/text_field.h>
#include <ui/theme_manager.h>

#include <chrono>
#include <cstddef>
#include <atomic>
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

/// @brief 模型选项。
struct model_option {
    std::string display_name;
    chat_client::config cfg;
};

/// @brief 后台初始加载结果。
struct initial_load_result {
    std::vector<conversation_meta> convos;  ///< 全部会话元信息（按 updated_at 倒序）。
    std::string uuid;                       ///< 需要打开的会话 UUID，空表示无会话。
    chat_archive archive;                   ///< 该会话的完整存档。
    bool loaded = false;                    ///< 会话存档是否加载成功。
};

/**
 * @brief 智能体标签页。
 */
class agent_tab : public tab {
public:
    explicit agent_tab(std::shared_ptr<chat_client> client,
                       std::shared_ptr<chat_store> store = nullptr);
    ~agent_tab() override {
        *alive_ = false;
        if (pending_.valid()) pending_.wait();
        if (initial_load_.valid()) initial_load_.wait();
        if (on_conversation_done) on_conversation_done(this);
        if (on_destroyed) on_destroyed();
    }

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void layout() override;
    void tick(float dt_ms) override;
    void on_activate() override;
    widget* hit_test_hover(float x, float y) const override;

    /** @brief 添加一条消息到显示列表。 */
    void add_message(const std::string& role, const std::string& content);

    /**
     * @brief 一轮对话完成后的回调。
     * @param tab 回调发起方的标签页。
     */
    std::function<void(agent_tab*)> on_conversation_done;

    /// @brief 标签页被关闭时回调，供持有者清理自身对该标签页的引用。
    std::function<void()> on_destroyed;

    /// @brief 设置可选模型列表。
    void set_models(std::vector<model_option> models);

    /// @brief 等待后台初始加载完成（释放存储前调用，避免访问已释放的存储）。
    void wait_initial_load();

    /// @brief 是否可以继续生成。
    bool can_continue() const { return continue_btn_ != nullptr; }

    /// @brief 当前是否自动审批敏感工具。
    bool auto_approve() const { return auto_approve_on_.load(); }

private:
    // 共享所有权：后台 run 线程在扩展关闭（client_/store_ 释放）后仍安全使用。
    std::shared_ptr<chat_client> client_;
    std::shared_ptr<chat_store>  store_;

    split_pane* split_pane_  = nullptr; 
    container* list_pane_   = nullptr;
    container* list_header_ = nullptr;
    container* list_scroll_ = nullptr;
    container* chat_pane_   = nullptr;
    container* back_bar_    = nullptr;
    container* scroll_      = nullptr;
    container* settings_bar_ = nullptr;
    container* input_bar_   = nullptr;
    container* msg_list_    = nullptr;
    todo_view* todo_view_   = nullptr;

    button*     back_btn_     = nullptr;
    button*     toggle_list_  = nullptr;
    button*     new_btn_      = nullptr;
    combo_box*  model_combo_  = nullptr;
    combo_box*  reasoning_combo_ = nullptr;
    checkbox*   auto_approve_ = nullptr;
    label*      token_label_  = nullptr;

    collapsible* todo_capsule_ = nullptr;
    size_t todo_count_ = 0;
    bool todo_has_ = false;

    text_field* input_   = nullptr;
    button*     send_btn_ = nullptr;

    std::vector<display_message> messages_;
    std::string current_uuid_;
    bool list_rebuild_pending_ = false;
    bool layout_batch_ = false;  ///< 批量布局期间抑制逐条 relayout。

    bool landscape_      = true;
    bool list_collapsed_ = false;
    bool show_chat_      = true;
    float list_expand_ratio_ = 0.3f;

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

    /// @brief 停止标志（共享原子）：后台线程只读共享指针指向的原子，避免析构后访问成员。
    std::shared_ptr<std::atomic<bool>> stop_flag_ = std::make_shared<std::atomic<bool>>(false);
    std::atomic<bool> supplement_requested_ = false;
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
    markdown* last_label_   = nullptr;
    size_t stream_msg_index_ = 0;
    bool streamed_content_ = false;

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

    void new_conversation(bool to_chat = true);
    void open_conversation(const std::string& uuid, bool to_chat = true);
    void delete_conversation(const std::string& uuid);
    /** @brief 重建会话列表。preset 非空时直接使用该列表，避免重复读盘。 */
    void rebuild_conversation_list(const std::vector<conversation_meta>* preset = nullptr);
    /** @brief 将后台初始加载结果应用到 UI（UI 线程调用）。 */
    void apply_initial_load(const initial_load_result& res);
    /** @brief 请求在下一帧 tick 重建会话列表。 */
    void request_list_rebuild();
    void clear_messages();
    void save_current();

    std::string rename_uuid_;
    input_dialog* rename_dialog_ = nullptr;
    void begin_rename(const std::string& uuid, const std::string& name);
    void commit_rename_text(const std::string& text);

    /** @brief 在工具胶囊内展示内联 允许/拒绝 审批。 */
    void show_inline_approval();
    /** @brief 审批结束后移除胶囊内的审批按钮。 */
    void remove_inline_approval();

    /** @brief 布局会话列表面板内部。 */
    void layout_list_internal();
    /** @brief 布局聊天面板内部。 */
    void layout_chat_internal();

    /** @brief 创建一条消息气泡并添加到列表，返回气泡控件指针。 */
    markdown* append_bubble(const display_message& msg);
    /**
     * @brief 处理积压的流式事件。
     * @param max_events 每帧最多处理的事件数。
     */
    void process_stream_events(size_t max_events = 0);
    void relayout_scroll_repaint();
    /** @brief 发送当前输入内容。 */
    void send();
    /** @brief 启动一次对话。 */
    void start_send(const std::string& text, bool show_user);
    /** @brief 基于当前历史启动一轮生成。 */
    void begin_run();
    /** @brief 继续生成。 */
    void continue_generation();
    /** @brief 在消息列表末尾添加“继续生成”按钮。 */
    void add_continue_button();
    /** @brief 移除“继续生成”按钮。 */
    void remove_continue_button();
    /** @brief 根据 waiting_ 更新发送/停止按钮文字。 */
    void update_send_button();
    /** @brief 刷新 token 用量标签。 */
    void update_token_label();

    /** @brief 设置思考等级并更新下拉框高亮。 */
    void set_reasoning_level(reasoning_level l);
    /** @brief 依据当前 provider 支持能力刷新思考挡位下拉框。 */
    void refresh_reasoning_combo();
    /** @brief 刷新待办胶囊。 */
    void update_todo_capsule();
    /** @brief 创建一个胶囊，返回其指针。 */
    collapsible* create_capsule(const std::string& summary);
    /** @brief 追加一个工具调用胶囊，返回其指针。 */
    collapsible* append_tool_capsule(const std::string& summary, const std::string& args);
    /** @brief 把工具结果填入当前胶囊内容。 */
    void append_tool_result(const std::string& text);
    /** @brief 从历史记录重建思考/工具胶囊。 */
    void restore_history_ui(const std::vector<chat_message>& msgs);

    /** @brief 后台线程回调：征询用户批准，阻塞直到 UI 决定或停止。 */
    bool approve_request(const tool_call& tc);
    /** @brief UI 线程：用户已决定，解除后台阻塞。 */
    void resolve_approval(bool ok);
};

} // namespace agent
} // namespace spiration
