/**
 * @file agent_tab.h
 * @brief 智能体标签页控件。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/chat.h>

#include <ui/button.h>
#include <ui/collapsible.h>
#include <ui/container.h>
#include <ui/layout.h>
#include <ui/markdown.h>
#include <ui/tab_bar.h>
#include <ui/text_field.h>
#include <ui/theme_manager.h>

#include <chrono>
#include <cstddef>
#include <atomic>
#include <deque>
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

/**
 * @brief 智能体标签页。
 */
class agent_tab : public tab {
public:
    explicit agent_tab(chat_client* client = nullptr);
    ~agent_tab() override { *alive_ = false; }

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void layout() override;
    void tick(float dt_ms) override;
    void on_activate() override;

    /** @brief 添加一条消息到显示列表。 */
    void add_message(const std::string& role, const std::string& content);

private:
    chat_client* client_ = nullptr;
    container*   scroll_ = nullptr;
    container*    msg_list_ = nullptr;
    text_field*   input_ = nullptr;
    button*       send_btn_ = nullptr;
    std::vector<display_message> messages_;

    reasoning_level reasoning_ = reasoning_level::standard;
    std::vector<button*> reasoning_btns_;
    collapsible* tool_capsule_ = nullptr;
    container* tool_capsule_scroll_ = nullptr;
    collapsible* thinking_capsule_ = nullptr;
    container* thinking_scroll_ = nullptr;
    markdown* thinking_label_ = nullptr;

    std::future<chat_response> pending_;
    bool waiting_ = false;

    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    std::atomic<bool> stop_requested_ = false;
    /// 补充消息已排队（软让出）：当前轮结束后不再继续，直接发送排队消息。
    std::atomic<bool> supplement_requested_ = false;
    std::deque<std::string> queued_inputs_;
    button* continue_btn_ = nullptr;

    struct stream_event {
        enum class type { delta, reasoning, tool_call, tool_result };
        type t = type::delta;
        std::string text;
    };
    std::mutex stream_mutex_;
    std::deque<stream_event> stream_events_;
    markdown* stream_label_ = nullptr;
    markdown* last_label_   = nullptr;
    /// 当前流式助手消息在 messages_ 中的下标（补充消息会追加在它之后，不能依赖 back()）。
    size_t stream_msg_index_ = 0;

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
    /** @brief 继续生成。 */
    void continue_generation();
    /** @brief 在消息列表末尾添加“继续生成”按钮。 */
    void add_continue_button();
    /** @brief 移除“继续生成”按钮。 */
    void remove_continue_button();
    /** @brief 根据 waiting_ 更新发送/停止按钮文字。 */
    void update_send_button();

    /** @brief 设置思考等级并更新按钮高亮。 */
    void set_reasoning_level(reasoning_level l);
    /** @brief 刷新思考等级按钮高亮。 */
    void update_reasoning_buttons();
    /** @brief 创建一个胶囊，返回其指针。 */
    collapsible* create_capsule(const std::string& summary);
    /** @brief 追加一个工具调用胶囊，返回其指针。 */
    collapsible* append_tool_capsule(const std::string& summary);
    /** @brief 把工具结果填入当前胶囊内容。 */
    void append_tool_result(const std::string& text);
};

} // namespace agent
} // namespace spiration
