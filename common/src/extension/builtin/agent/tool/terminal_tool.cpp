/**
 * @file terminal_tool.cpp
 * @brief 终端工具集实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/terminal_tool.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>

namespace spiration {
namespace agent {

terminal_manager& terminal_manager::instance() {
    static terminal_manager inst;
    return inst;
}

std::string terminal_manager::create(const std::string& shell) {
    std::unique_ptr<terminal_session> s = create_terminal_session(shell);
    if (!s) return "[error] failed to create terminal session";
    std::lock_guard<std::mutex> lk(mtx_);
    std::string id = "t" + std::to_string(next_id_++);
    sessions_[id] = std::move(s);
    console::info("terminal", "created session %s", id.c_str());
    return id;
}

std::string terminal_manager::write(const std::string& id, const std::string& data, bool newline) {
    terminal_session* s;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = sessions_.find(id);
        if (it == sessions_.end()) return "[error] terminal not found: " + id;
        s = it->second.get();
    }
    if (!s->is_alive()) return "[error] terminal " + id + " is not alive: " + s->error();
    std::string payload = data;
    if (newline) payload += "\n";
    s->write(payload);
    return "ok";
}

std::string terminal_manager::read(const std::string& id, size_t from_bottom, size_t to_bottom) {
    terminal_session* s;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = sessions_.find(id);
        if (it == sessions_.end()) return "[error] terminal not found: " + id;
        s = it->second.get();
    }
    if (s->line_count() == 0) return "[empty]";
    std::vector<std::string> lines = s->read_window(from_bottom, to_bottom);
    if (lines.empty()) return "[empty]";
    std::string out;
    for (const auto& l : lines) {
        out += l;
        out += "\n";
    }
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

void terminal_manager::close_all() {
    std::lock_guard<std::mutex> lk(mtx_);
    sessions_.clear();
}

std::string create_terminal_tool::description() const {
    return "Create an interactive terminal session that stays alive between calls. "
           "Returns a terminal_id used with write_terminal and read_terminal. "
           "The session keeps its working directory and environment across commands.";
}

std::string create_terminal_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "shell": {
            "type": "string",
            "description": "Optional shell path. Empty means the system default shell."
        }
    }
})";
}

std::string create_terminal_tool::execute(const std::string& args_json) {
    std::string shell;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        shell = j.value("shell", "");
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    return terminal_manager::instance().create(shell);
}

std::string write_terminal_tool::description() const {
    return "Write input to a terminal created by create_terminal. "
           "Provide terminal_id and input text. "
           "A newline (Enter) is appended by default; set newline=false to send raw text.";
}

std::string write_terminal_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "terminal_id": {
            "type": "string",
            "description": "The terminal id returned by create_terminal."
        },
        "input": {
            "type": "string",
            "description": "The text to send to the terminal."
        },
        "newline": {
            "type": "boolean",
            "description": "Append a newline after the input. Default true."
        }
    },
    "required": ["terminal_id", "input"]
})";
}

std::string write_terminal_tool::execute(const std::string& args_json) {
    std::string id, input;
    bool newline = true;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        id = j.value("terminal_id", "");
        input = j.value("input", "");
        newline = j.value("newline", true);
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (id.empty() || input.empty()) return "[error] missing 'terminal_id' or 'input'";
    return terminal_manager::instance().write(id, input, newline);
}

std::string read_terminal_tool::description() const {
    return "Read the output window of a terminal created by create_terminal. "
           "Lines are counted from the bottom: line 1 is the newest. "
           "Read from_line to to_line (inclusive) as a window; "
           "returned lines are in chronological order.";
}

std::string read_terminal_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "terminal_id": {
            "type": "string",
            "description": "The terminal id returned by create_terminal."
        },
        "from_line": {
            "type": "integer",
            "description": "Start line counting from the bottom (1 = newest). Default 1."
        },
        "to_line": {
            "type": "integer",
            "description": "End line counting from the bottom. Default 50."
        }
    },
    "required": ["terminal_id"]
})";
}

std::string read_terminal_tool::execute(const std::string& args_json) {
    std::string id;
    size_t from_line = 1;
    size_t to_line = 50;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        id = j.value("terminal_id", "");
        from_line = j.value("from_line", from_line);
        to_line = j.value("to_line", to_line);
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (id.empty()) return "[error] missing 'terminal_id'";
    return terminal_manager::instance().read(id, from_line, to_line);
}

} // namespace agent
} // namespace spiration
