/**
 * @file todo_tool.cpp
 * @brief 待办事项工具实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/todo_tool.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>

namespace spiration {
namespace agent {

todo_status todo_status_from_string(const std::string& s) {
    if (s == "in_progress") return todo_status::in_progress;
    if (s == "completed")   return todo_status::completed;
    return todo_status::pending;
}

std::string todo_status_to_string(todo_status s) {
    switch (s) {
        case todo_status::pending:     return "pending";
        case todo_status::in_progress: return "in_progress";
        case todo_status::completed:   return "completed";
    }
    return "pending";
}

todo_store& todo_store::instance() {
    static todo_store inst;
    return inst;
}

std::vector<todo_item> todo_store::items() const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = lists_.find(current_);
    if (it == lists_.end()) return {};
    return it->second;
}

void todo_store::set(const std::vector<todo_item>& incoming) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<todo_item> prev = lists_[current_];
    std::vector<todo_item> next;
    next.reserve(incoming.size());
    for (const auto& in : incoming) {
        todo_item item = in;
        auto it = std::find_if(prev.begin(), prev.end(),
                               [&](const todo_item& e) { return e.content == in.content; });
        if (it != prev.end()) {
            item.id = it->id;
        } else if (item.id.empty()) {
            item.id = "t" + std::to_string(next_id_++);
        }
        next.push_back(std::move(item));
    }
    lists_[current_] = std::move(next);
    ++version_;
}

void todo_store::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    lists_[current_].clear();
    ++version_;
}

void todo_store::set_current_uuid(const std::string& uuid) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (current_ == uuid) return;
    current_ = uuid;
    ++version_;
}

void todo_store::remove(const std::string& uuid) {
    std::lock_guard<std::mutex> lk(mtx_);
    lists_.erase(uuid);
    if (current_ == uuid) current_.clear();
    ++version_;
}

std::string todo_tool::description() const {
    return "Maintain a trackable todo list for the current task. "
           "Pass the full desired list in 'todos', each item has 'content' "
           "and an optional 'status' (pending / in_progress / completed). "
           "Items are matched by content to keep stable ids. "
           "Use this to plan multi-step work and show progress.";
}

std::string todo_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "todos": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "content": {
                        "type": "string",
                        "description": "Todo item description."
                    },
                    "status": {
                        "type": "string",
                        "enum": ["pending", "in_progress", "completed"],
                        "description": "Item status. Default pending."
                    }
                },
                "required": ["content"]
            },
            "description": "The full todo list to store."
        }
    },
    "required": ["todos"]
})";
}

std::string todo_tool::execute(const std::string& args_json) {
    std::vector<todo_item> items;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        if (!j.contains("todos") || !j["todos"].is_array())
            return "[error] missing 'todos' array";
        for (auto& t : j["todos"]) {
            if (!t.is_object()) continue;
            todo_item item;
            item.id      = t.value("id", "");
            item.content = t.value("content", "");
            item.status  = todo_status_from_string(t.value("status", "pending"));
            if (item.content.empty()) continue;
            items.push_back(std::move(item));
        }
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }

    todo_store::instance().set(items);

    size_t pending = 0, in_progress = 0, completed = 0;
    for (const auto& it : items) {
        switch (it.status) {
            case todo_status::pending:     ++pending;     break;
            case todo_status::in_progress: ++in_progress; break;
            case todo_status::completed:   ++completed;   break;
        }
    }
    console::info("extension/agent/todo", "todo updated: %zu items (%zu pending, %zu in progress, %zu completed)",
                  items.size(), pending, in_progress, completed);
    return "ok (" + std::to_string(items.size()) + " items, " +
           std::to_string(pending) + " pending, " +
           std::to_string(in_progress) + " in progress, " +
           std::to_string(completed) + " completed)";
}

} // namespace agent
} // namespace spiration
