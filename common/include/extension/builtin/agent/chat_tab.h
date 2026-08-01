/**
 * @file chat_tab.h
 * @brief 智能体标签页控件。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/chat.h>

#include <ui/button.h>
#include <ui/layout.h>
#include <ui/markdown.h>
#include <ui/scroll_view.h>
#include <ui/tab_bar.h>
#include <ui/text_field.h>
#include <ui/theme_manager.h>

#include <chrono>
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
class chat_tab : public tab {
public:
    explicit chat_tab(chat_client* client = nullptr);

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void layout() override;
    void tick(float dt_ms) override;
    void on_activate() override;

    /** @brief 添加一条消息到显示列表。 */
    void add_message(const std::string& role, const std::string& content);

private:
    chat_client* client_ = nullptr;
    scroll_view* scroll_ = nullptr;
    container*    msg_list_ = nullptr;
    text_field*   input_ = nullptr;
    button*       send_btn_ = nullptr;
    std::vector<display_message> messages_;

    std::future<chat_response> pending_;
    bool waiting_ = false;

    struct stream_event {
        enum class type { delta, tool_call, tool_result };
        type t = type::delta;
        std::string text;
    };
    std::mutex stream_mutex_;
    std::deque<stream_event> stream_events_;
    markdown* stream_label_ = nullptr;
    markdown* last_label_   = nullptr;

    /** @brief 创建一条消息气泡并添加到列表，返回气泡控件指针。 */
    markdown* append_bubble(const display_message& msg);
    /** @brief 处理积压的流式事件。 */
    void process_stream_events();
    void relayout_scroll_repaint();
    /** @brief 发送当前输入内容。 */
    void send();
};

} // namespace agent
} // namespace spiration
