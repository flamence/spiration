/**
 * @file memory_tool.cpp
 * @brief 记忆工具实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/agent/tool/memory_tool.h>

#include <nlohmann/json.hpp>

#include <exception>
#include <fmt/format.h>

namespace spiration {
namespace agent {

std::string memory_tool::description() const {
    return "Read or update persistent memory (memory.md) for the current conversation. "
           "Actions: \"read\" returns the current memory; \"write\" replaces it with \"content\"; "
           "\"append\" appends \"content\"; \"clear\" empties it. "
           "Use it to remember important context across turns.";
}

nlohmann::json memory_tool::parameters() const {
    return nlohmann::json::parse(R"({
    "type": "object",
    "properties": {
        "action": {
            "type": "string",
            "enum": ["read", "write", "append", "clear"],
            "description": "What to do with memory. Default read."
        },
        "content": {
            "type": "string",
            "description": "Memory content for write/append."
        }
    },
    "required": ["action"]
})");
}

std::string memory_tool::execute(const std::string& args_json) {
    std::string action = "read";
    std::string content;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        action  = j.value("action", "read");
        content = j.value("content", "");
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }

    if (!store_ || !current_uuid_) {
        return "[error] memory store not bound";
    }
    std::string uuid = current_uuid_();
    if (uuid.empty()) return "[error] no active conversation";

    if (action == "read") {
        std::string mem = store_->load_memory(uuid);
        return mem.empty() ? "[empty] no memory saved" : mem;
    }
    if (action == "write") {
        store_->save_memory(uuid, content);
        application::instance()->logger()->log({logging::level::info, fmt::format("wrote memory ({} chars)", content.size()), "extension/agent/memory"});
        return "ok";
    }
    if (action == "append") {
        std::string mem = store_->load_memory(uuid);
        if (!mem.empty() && mem.back() != '\n') mem += "\n";
        mem += content;
        store_->save_memory(uuid, mem);
        application::instance()->logger()->log({logging::level::info, fmt::format("appended memory ({} chars)", content.size()), "extension/agent/memory"});
        return "ok";
    }
    if (action == "clear") {
        store_->save_memory(uuid, "");
        return "ok";
    }
    return "[error] unknown action: " + action;
}

} // namespace agent
} // namespace spiration
