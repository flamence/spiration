/**
 * @file ohos_stubs.cpp
 * @brief OHOS 平台桩函数实现
 * @author clk
 */

#include <window/window.h>
#include <io/file_dialog.h>
#include <extension/builtin/agent/chat.h>
#include <utils/console.h>
#include <string>
#include <vector>

namespace spiration {

// ============================================================================
// Window 桩实现
// ============================================================================

std::shared_ptr<window> window::create() {
    console::warning("window", "Window creation not fully implemented on OHOS platform");
    return nullptr;
}

std::shared_ptr<window> window::create(const window_params& params) {
    console::warning("window", "Window creation not fully implemented on OHOS platform");
    (void)params;
    return nullptr;
}

// ============================================================================
// IO 桩实现
// ============================================================================

namespace io {

std::string open_file(const std::string& title, const std::string& filter, const std::vector<std::string>& patterns) {
    console::warning("io", "File dialog not implemented on OHOS platform");
    (void)title;
    (void)filter;
    (void)patterns;
    return "";
}

std::string save_file(const std::string& title, const std::string& filter, const std::vector<std::string>& patterns) {
    console::warning("io", "File dialog not implemented on OHOS platform");
    (void)title;
    (void)filter;
    (void)patterns;
    return "";
}

} // namespace io

// ============================================================================
// Agent 桩实现
// ============================================================================

namespace agent {

chat_client::chat_client(const config& cfg) : cfg_(cfg) {
    console::warning("chat", "Chat client not fully implemented on OHOS platform");
}

chat_client::~chat_client() = default;

void chat_client::set_system_prompt(const std::string& prompt) {
    // 系统提示存储在 history_ 的第一条消息中
    if (history_.empty() || history_[0].role != "system") {
        history_.insert(history_.begin(), {"system", prompt, "", "", {}});
    } else {
        history_[0].content = prompt;
    }
}

void chat_client::add_message(const chat_message& msg) {
    history_.push_back(msg);
}

void chat_client::register_tool(tool* t) {
    if (t) tools_.push_back(t);
}

chat_response chat_client::send() {
    console::warning("chat", "Chat send not implemented on OHOS platform");
    return chat_response{};
}

chat_response chat_client::run(int max_rounds, const chat_events& events) {
    console::warning("chat", "Chat run not implemented on OHOS platform");
    return chat_response{};
}

} // namespace agent

} // namespace spiration
