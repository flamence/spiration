/**
 * @file chat_store.cpp
 * @brief 对话记录存取实现。
 * @author clk
 */

#include <extension/builtin/agent/chat_store.h>
#include <utils/console.h>
#include <utils/path.h>
#include <utils/platform.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

namespace {

/// @brief 将 chat_message 序列化为 JSON。
nlohmann::json message_to_json(const chat_message& m) {
    nlohmann::json j;
    j["role"]             = m.role;
    j["content"]          = m.content;
    j["reasoning_content"] = m.reasoning_content;
    j["tool_call_id"]     = m.tool_call_id;
    j["name"]             = m.name;
    nlohmann::json tcs = nlohmann::json::array();
    for (const auto& tc : m.tool_calls) {
        tcs.push_back({{"id", tc.id},
                       {"function_name", tc.function_name},
                       {"arguments", tc.arguments}});
    }
    j["tool_calls"] = std::move(tcs);
    return j;
}

/// @brief 从 JSON 解析 chat_message。
chat_message message_from_json(const nlohmann::json& j) {
    chat_message m;
    m.role             = j.value("role", "");
    m.content          = j.value("content", "");
    m.reasoning_content = j.value("reasoning_content", "");
    m.tool_call_id     = j.value("tool_call_id", "");
    m.name             = j.value("name", "");
    if (j.contains("tool_calls") && j["tool_calls"].is_array()) {
        for (const auto& tc : j["tool_calls"]) {
            tool_call c;
            c.id            = tc.value("id", "");
            c.function_name = tc.value("function_name", "");
            c.arguments     = tc.value("arguments", "");
            m.tool_calls.push_back(std::move(c));
        }
    }
    return m;
}

std::string now_string() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::string default_name() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d",
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    return buf;
}

std::string read_text_file(const std::string& path) {
    std::ifstream f(path::u8path(path), std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool write_text_file(const std::string& path, const std::string& content) {
    auto p = path::u8path(path);
    std::error_code ec;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(f);
}

} // namespace

chat_store::chat_store(std::string base_dir) : base_dir_(std::move(base_dir)) {
    platform::create_directory(base_dir_);
    platform::create_directory(chat_dir());
}

std::string chat_store::make_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(gen), b = dist(gen);
    a = (a & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull;
    b = (b & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;

    char buf[40];
    std::snprintf(buf, sizeof(buf),
                  "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<unsigned>(a >> 32),
                  static_cast<unsigned>((a >> 16) & 0xFFFF),
                  static_cast<unsigned>(a & 0xFFFF),
                  static_cast<unsigned>(b >> 48),
                  static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFull));
    return buf;
}

std::string chat_store::chat_dir() const {
    return platform::join_path(base_dir_, "chat");
}

std::string chat_store::conversation_dir(const std::string& uuid) const {
    return platform::join_path(chat_dir(), uuid);
}

std::string chat_store::current_uuid() const {
    std::lock_guard<std::mutex> lk(uuid_mtx_);
    return current_uuid_;
}

void chat_store::set_current_uuid(const std::string& uuid) {
    std::lock_guard<std::mutex> lk(uuid_mtx_);
    current_uuid_ = uuid;
}

std::string chat_store::create(const std::string& name) {
    std::string uuid = make_uuid();
    std::string dir = conversation_dir(uuid);
    if (!platform::create_directory(dir)) {
        console::error("extension/agent/chat_store", "create conversation dir failed: %s", dir.c_str());
        return {};
    }

    nlohmann::json meta;
    meta["uuid"]       = uuid;
    meta["name"]       = name.empty() ? default_name() : name;
    meta["created_at"] = now_string();
    meta["updated_at"] = meta["created_at"];
    meta["provider"]   = "";
    meta["model"]      = "";
    meta["message_count"] = 0;
    meta["can_continue"]  = false;
    meta["auto_approve"]  = false;
    meta["tokens_in"]     = 0;
    meta["tokens_out"]    = 0;
    meta["todos"]         = nlohmann::json::array();

    if (!write_text_file(platform::join_path(dir, "meta.json"),
                         meta.dump(2, ' ', false, nlohmann::json::error_handler_t::replace))) {
        console::error("extension/agent/chat_store", "create meta.json failed: %s", uuid.c_str());
        return {};
    }
    console::info("extension/agent/chat_store", "created conversation '%s' (%s)",
                  meta["name"].get<std::string>().c_str(), uuid.c_str());
    return uuid;
}

bool chat_store::save(const chat_archive& archive) {
    if (archive.uuid.empty()) return false;
    std::string dir = conversation_dir(archive.uuid);
    if (!platform::create_directory(dir)) return false;

    nlohmann::json meta;
    meta["uuid"]       = archive.uuid;
    meta["name"]       = archive.name.empty() ? default_name() : archive.name;
    meta["created_at"] = archive.created_at;
    {
        std::string existing_updated;
        try {
            nlohmann::json old = nlohmann::json::parse(
                read_text_file(platform::join_path(dir, "meta.json")));
            existing_updated = old.value("updated_at", "");
        } catch (...) {}
        meta["updated_at"] = existing_updated.empty() ? now_string() : existing_updated;
    }
    meta["provider"]   = archive.provider;
    meta["model"]      = archive.model;
    meta["message_count"] = archive.messages.size();
    meta["can_continue"]  = archive.can_continue;
    meta["auto_approve"]  = archive.auto_approve;
    meta["tokens_in"]     = archive.tokens_in;
    meta["tokens_out"]    = archive.tokens_out;
    nlohmann::json todos = nlohmann::json::array();
    for (const auto& t : archive.todos) {
        todos.push_back({{"id", t.id}, {"content", t.content},
                         {"status", todo_status_to_string(t.status)}});
    }
    meta["todos"] = std::move(todos);

    nlohmann::json root;
    root["provider"] = archive.provider;
    root["model"]    = archive.model;
    nlohmann::json msgs = nlohmann::json::array();
    for (const auto& m : archive.messages) msgs.push_back(message_to_json(m));
    root["messages"] = std::move(msgs);

    nlohmann::json terms = nlohmann::json::array();
    for (const auto& t : archive.terminals) {
        nlohmann::json tj;
        tj["id"]    = t.id;
        tj["shell"] = t.shell;
        nlohmann::json lines = nlohmann::json::array();
        for (const auto& l : t.lines) lines.push_back(l);
        tj["lines"] = std::move(lines);
        terms.push_back(std::move(tj));
    }
    root["terminals"] = std::move(terms);

    if (!write_text_file(platform::join_path(dir, "messages.json"),
                         root.dump(2, ' ', false, nlohmann::json::error_handler_t::replace)))
        return false;
    if (!write_text_file(platform::join_path(dir, "meta.json"),
                         meta.dump(2, ' ', false, nlohmann::json::error_handler_t::replace)))
        return false;
    if (!archive.memory.empty())
        write_text_file(platform::join_path(dir, "memory.md"), archive.memory);

    console::info("extension/agent/chat_store", "saved conversation '%s' (%zu messages)",
                  archive.name.c_str(), archive.messages.size());
    return true;
}

bool chat_store::load(const std::string& uuid, chat_archive& archive) const {
    std::string dir = conversation_dir(uuid);

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(read_text_file(platform::join_path(dir, "messages.json")));
    } catch (...) {
        return false;
    }

    archive.uuid      = uuid;
    archive.provider  = root.value("provider", "");
    archive.model     = root.value("model", "");
    archive.messages.clear();
    if (root.contains("messages") && root["messages"].is_array()) {
        for (const auto& m : root["messages"])
            archive.messages.push_back(message_from_json(m));
    }
    archive.terminals.clear();
    if (root.contains("terminals") && root["terminals"].is_array()) {
        for (const auto& t : root["terminals"]) {
            terminal_snapshot snap;
            snap.id    = t.value("id", "");
            snap.shell = t.value("shell", "");
            if (t.contains("lines") && t["lines"].is_array()) {
                for (const auto& l : t["lines"])
                    snap.lines.push_back(l.get<std::string>());
            }
            archive.terminals.push_back(std::move(snap));
        }
    }

    try {
        nlohmann::json meta = nlohmann::json::parse(
            read_text_file(platform::join_path(dir, "meta.json")));
        archive.name       = meta.value("name", default_name());
        archive.created_at = meta.value("created_at", "");
        archive.can_continue = meta.value("can_continue", false);
        archive.auto_approve = meta.value("auto_approve", false);
        archive.tokens_in  = meta.value("tokens_in", 0LL);
        archive.tokens_out = meta.value("tokens_out", 0LL);
        archive.todos.clear();
        if (meta.contains("todos") && meta["todos"].is_array()) {
            for (const auto& t : meta["todos"]) {
                todo_item item;
                item.id      = t.value("id", "");
                item.content = t.value("content", "");
                item.status  = todo_status_from_string(t.value("status", "pending"));
                if (!item.content.empty()) archive.todos.push_back(std::move(item));
            }
        }
    } catch (...) {
        archive.name = default_name();
    }

    archive.memory = read_text_file(platform::join_path(dir, "memory.md"));
    return true;
}

bool chat_store::remove(const std::string& uuid) {
    if (uuid.empty()) return false;
    std::error_code ec;
    std::filesystem::remove_all(path::u8path(conversation_dir(uuid)), ec);
    bool ok = !ec;
    if (ok) {
        std::lock_guard<std::mutex> lk(uuid_mtx_);
        if (current_uuid_ == uuid) current_uuid_.clear();
    }
    console::info("extension/agent/chat_store", "removed conversation %s", uuid.c_str());
    return ok;
}

std::vector<conversation_meta> chat_store::list() const {
    std::vector<conversation_meta> out;
    for (const auto& entry : platform::list_directory(chat_dir())) {
        std::string dir = platform::join_path(chat_dir(), entry);
        if (!platform::file_exists(platform::join_path(dir, "meta.json"))) continue;

        conversation_meta meta;
        meta.uuid = entry;
        try {
            nlohmann::json j = nlohmann::json::parse(
                read_text_file(platform::join_path(dir, "meta.json")));
            meta.name          = j.value("name", entry);
            meta.created_at    = j.value("created_at", "");
            meta.updated_at    = j.value("updated_at", "");
            meta.provider      = j.value("provider", "");
            meta.model         = j.value("model", "");
            meta.message_count = j.value("message_count", 0);
        } catch (...) {
            meta.name = entry;
        }
        out.push_back(std::move(meta));
    }
    std::sort(out.begin(), out.end(), [](const conversation_meta& a, const conversation_meta& b) {
        return a.updated_at > b.updated_at;
    });
    return out;
}

std::string chat_store::load_memory(const std::string& uuid) const {
    if (uuid.empty()) return {};
    return read_text_file(platform::join_path(conversation_dir(uuid), "memory.md"));
}

bool chat_store::save_memory(const std::string& uuid, const std::string& content) {
    if (uuid.empty()) return false;
    return write_text_file(platform::join_path(conversation_dir(uuid), "memory.md"), content);
}

} // namespace agent
} // namespace spiration
