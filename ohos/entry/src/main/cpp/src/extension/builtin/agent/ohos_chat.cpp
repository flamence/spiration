/**
 * @file ohos_chat.cpp
 * @brief 聊天客户端实现。
 * @author clk
 */

#include <extension/builtin/agent/chat.h>
#include <utils/console.h>

namespace spiration {

namespace agent {

chat_client::chat_client(const config& cfg) : cfg_(cfg) {
    console::warning("chat", "Chat client not fully implemented on OHOS platform");
}

chat_client::~chat_client() = default;

void chat_client::configure(const config& c) {
    cfg_ = c;
}

std::string chat_client::provider_name() const {
    return cfg_.provider;
}

bool chat_client::switch_provider(const std::string& name) {
    if (name.empty()) return true;
    cfg_.provider = name;
    return true;
}

void chat_client::set_system_prompt(const std::string& prompt) {
    if (history_.empty() || history_[0].role != "system") {
        history_.insert(history_.begin(), {"system", prompt, "", "", {}});
    } else {
        history_[0].content = prompt;
    }
}

void chat_client::add_message(const chat_message& msg) {
    history_.push_back(msg);
}

void chat_client::clear_history() {
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) { return m.role != "system"; }),
        history_.end());
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