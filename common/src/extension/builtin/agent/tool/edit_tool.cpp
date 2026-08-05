/**
 * @file edit_tool.cpp
 * @brief 文件编辑工具集实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/edit_tool.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>

#include <utils/path.h>

namespace spiration {
namespace agent {

namespace {

bool write_file(const std::string& path, const std::string& content) {
    auto p = path::u8path(path);
    std::error_code ec;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(f);
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path::u8path(path), std::ios::binary);
    if (!f) return false;
    std::string buf;
    char chunk[65536];
    while (f.read(chunk, sizeof(chunk)) || f.gcount() > 0) {
        buf.append(chunk, static_cast<size_t>(f.gcount()));
    }
    out = std::move(buf);
    return true;
}

/// @brief 行尾风格。
enum class eol_style { lf, crlf };

/// @brief 检测文本的主流行尾风格。
eol_style detect_eol(const std::string& s) {
    size_t crlf = 0, lf = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') {
            if (i > 0 && s[i - 1] == '\r') ++crlf;
            else ++lf;
        }
    }
    return crlf > lf ? eol_style::crlf : eol_style::lf;
}

/// @brief 将 CRLF 统一为 LF，原地压缩。
void normalize_to_lf(std::string& s) {
    size_t w = 0;
    for (size_t r = 0; r < s.size(); ++r) {
        if (s[r] == '\r' && r + 1 < s.size() && s[r + 1] == '\n') continue;
        s[w++] = s[r];
    }
    s.resize(w);
}

/// @brief 将 LF 恢复为 CRLF，原地扩展。
void lf_to_crlf(std::string& s) {
    size_t count = 0;
    for (char c : s)
        if (c == '\n') ++count;
    if (count == 0) return;
    s.resize(s.size() + count);
    size_t w = s.size();
    for (size_t r = s.size() - count; r > 0;) {
        --r;
        char c = s[r];
        if (c == '\n') {
            s[--w] = '\n';
            s[--w] = '\r';
        } else {
            s[--w] = c;
        }
    }
}

/// @brief 按检测到的风格恢复行尾。
void restore_eol(std::string& s, eol_style eol) {
    if (eol == eol_style::crlf) lf_to_crlf(s);
}

} // namespace

std::string create_file_tool::description() const {
    return "Create a new file with the given content. "
           "Parent directories are created automatically if missing.";
}

std::string create_file_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "path": {
            "type": "string",
            "description": "File path to create."
        },
        "content": {
            "type": "string",
            "description": "File content. Default is empty."
        }
    },
    "required": ["path"]
})";
}

std::string create_file_tool::execute(const std::string& args_json) {
    std::string path, content;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        path = j.value("path", "");
        content = j.value("content", "");
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (path.empty()) return "[error] missing \"path\"";
    path = resolve_path(path);
    if (!write_file(path, content))
        return "[error] create_file failed: " + path;
    console::info("extension/agent/edit", "created file: %s (%zu bytes)", path.c_str(), content.size());
    return "ok";
}

std::string read_file_tool::description() const {
    return "Read a file and return its content. "
           "Inspect files before editing them. "
           "Use \"start_line\"/\"end_line\" (1-based, inclusive) to read a range; "
           "by default only the first 200 lines are returned.";
}

std::string read_file_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "path": {
            "type": "string",
            "description": "File path to read."
        },
        "start_line": {
            "type": "integer",
            "description": "1-based first line to read (inclusive). Default 1."
        },
        "end_line": {
            "type": "integer",
            "description": "1-based last line to read (inclusive). Default: min(last line, 200)."
        }
    },
    "required": ["path"]
})";
}

std::string read_file_tool::execute(const std::string& args_json) {
    std::string path;
    long start_line = 1;
    long end_line = 0;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        path = j.value("path", "");
        start_line = j.value("start_line", 1L);
        end_line = j.value("end_line", 0L);
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (path.empty()) return "[error] missing \"path\"";
    path = resolve_path(path);

    std::error_code ec;
    uintmax_t sz = std::filesystem::file_size(path::u8path(path), ec);
    if (ec) return "[error] read_file failed (stat): " + path;
    if (sz > 16u * 1024u * 1024u)
        return "[error] read_file: file too large (" + std::to_string(sz) + " bytes)";

    std::string data;
    if (!read_file(path, data)) return "[error] read_file failed: " + path;

    std::vector<std::string> lines;
    {
        std::string cur;
        for (char c : data) {
            if (c == '\n') {
                if (!cur.empty() && cur.back() == '\r') cur.pop_back();
                lines.push_back(std::move(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) lines.push_back(std::move(cur));
    }

    constexpr long MAX_LINES = 200;
    if (start_line < 1) start_line = 1;
    if (end_line <= 0 || end_line > static_cast<long>(lines.size()))
        end_line = static_cast<long>(lines.size());
    if (end_line - start_line + 1 > MAX_LINES) end_line = start_line + MAX_LINES - 1;

    std::string out;
    for (long ln = start_line; ln <= end_line; ++ln) {
        out += lines[static_cast<size_t>(ln - 1)];
        out += "\n";
    }
    if (end_line < static_cast<long>(lines.size())) {
        out += "... (剩余 " + std::to_string(lines.size() - end_line) + " 行未显示)\n";
    }
    console::info("extension/agent/edit", "read file: %s (lines %ld-%ld / %zu)",
                  path.c_str(), start_line, end_line, lines.size());
    return out;
}

std::string create_directory_tool::description() const {
    return "Create a directory (and any missing parents). No-op if it already exists.";
}

std::string create_directory_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "path": {
            "type": "string",
            "description": "Directory path to create."
        }
    },
    "required": ["path"]
})";
}

std::string create_directory_tool::execute(const std::string& args_json) {
    std::string path;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        path = j.value("path", "");
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (path.empty()) return "[error] missing \"path\"";
    path = resolve_path(path);
    std::error_code ec;
    std::filesystem::create_directories(path::u8path(path), ec);
    if (ec) return "[error] create_directory failed: " + path + " (" + ec.message() + ")";
    console::info("extension/agent/edit", "created directory: %s", path.c_str());
    return "ok";
}

std::string edit_file_tool::description() const {
    return "Edit a file. Provide \"content\" to rewrite the whole file, "
           "or \"search\"+\"replace\" to do a targeted find-and-replace "
           "(replace_all=true replaces every occurrence, default replaces the first).";
}

std::string edit_file_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "path": {
            "type": "string",
            "description": "File path to edit."
        },
        "content": {
            "type": "string",
            "description": "If provided, the whole file is overwritten with this content."
        },
        "search": {
            "type": "string",
            "description": "Text to find (used with \"replace\")."
        },
        "replace": {
            "type": "string",
            "description": "Replacement text. Default is empty (removes the match)."
        },
        "replace_all": {
            "type": "boolean",
            "description": "Replace every occurrence instead of only the first. Default false."
        }
    },
    "required": ["path"]
})";
}

std::string edit_file_tool::execute(const std::string& args_json) {
    std::string path, content, search, replace;
    bool has_content = false, has_search = false, replace_all = false;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        path = j.value("path", "");
        has_content = j.contains("content");
        content = j.value("content", "");
        has_search = j.contains("search");
        search = j.value("search", "");
        replace = j.value("replace", "");
        replace_all = j.value("replace_all", false);
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (path.empty()) return "[error] missing \"path\"";
    path = resolve_path(path);

    if (has_content) {
        // 先统一为 LF，再按目标行尾恢复，避免 content 自带 CRLF 时重复加 \r（\r\r\n）。
        std::string out = content;
        eol_style target = eol_style::lf;
        std::string old;
        if (read_file(path, old)) target = detect_eol(old);
        normalize_to_lf(out);
        if (target == eol_style::crlf) lf_to_crlf(out);
        if (!write_file(path, out))
            return "[error] edit_file failed (write): " + path;
        console::info("extension/agent/edit", "edited file (full rewrite): %s (%zu bytes)", path.c_str(), out.size());
        return "ok";
    }

    if (has_search) {
        std::string data;
        if (!read_file(path, data)) return "[error] edit_file failed (read): " + path;
        if (search.empty()) return "[error] \"search\" must not be empty";
        eol_style eol = detect_eol(data);
        normalize_to_lf(data);
        normalize_to_lf(search);
        normalize_to_lf(replace);
        if (replace_all) {
            size_t p = 0;
            size_t count = 0;
            while ((p = data.find(search, p)) != std::string::npos) {
                data.replace(p, search.size(), replace);
                p += replace.size();
                ++count;
            }
            if (count == 0) return "[error] \"search\" text not found in " + path;
            console::info("extension/agent/edit", "replaced %zu occurrence(s) in %s", count, path.c_str());
        } else {
            size_t pos = data.find(search);
            if (pos == std::string::npos) return "[error] \"search\" text not found in " + path;
            data.replace(pos, search.size(), replace);
        }
        restore_eol(data, eol);
        if (!write_file(path, data)) return "[error] edit_file failed (write): " + path;
        return "ok";
    }

    return "[error] no edit operation: provide \"content\" or \"search\"";
}

std::string rename_tool::description() const {
    return "Rename or move a file or directory to a new path.";
}

std::string rename_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "path": {
            "type": "string",
            "description": "Current path of the file or directory."
        },
        "new_path": {
            "type": "string",
            "description": "Target path after rename/move."
        }
    },
    "required": ["path", "new_path"]
})";
}

std::string rename_tool::execute(const std::string& args_json) {
    std::string path, new_path;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        path = j.value("path", "");
        new_path = j.value("new_path", "");
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (path.empty() || new_path.empty()) return "[error] missing \"path\" or \"new_path\"";
    path = resolve_path(path);        // 相对路径 → 会话工作目录
    new_path = resolve_path(new_path);
    std::error_code ec;
    std::filesystem::rename(path::u8path(path), path::u8path(new_path), ec);
    if (ec) return "[error] rename failed: " + path + " -> " + new_path + " (" + ec.message() + ")";
    console::info("extension/agent/edit", "renamed: %s -> %s", path.c_str(), new_path.c_str());
    return "ok";
}

std::string delete_tool::description() const {
    return "Delete a file or directory. Directories are removed recursively.";
}

std::string delete_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "path": {
            "type": "string",
            "description": "Path of the file or directory to delete."
        }
    },
    "required": ["path"]
})";
}

std::string delete_tool::execute(const std::string& args_json) {
    std::string path;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        path = j.value("path", "");
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (path.empty()) return "[error] missing \"path\"";
    path = resolve_path(path);  // 相对路径 → 会话工作目录
    std::error_code ec;
    auto p = path::u8path(path);
    bool is_dir = std::filesystem::is_directory(p, ec);
    if (ec) return "[error] delete failed (stat): " + path + " (" + ec.message() + ")";
    if (is_dir) std::filesystem::remove_all(p, ec);
    else std::filesystem::remove(p, ec);
    if (ec) return "[error] delete failed: " + path + " (" + ec.message() + ")";
    console::info("extension/agent/edit", "deleted: %s", path.c_str());
    return "ok";
}

} // namespace agent
} // namespace spiration
