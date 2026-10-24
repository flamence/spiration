/**
 * @file agent.cpp
 * @brief C API 适配层 · 智能体可选接口（对话补全 provider 注册）。
 * @author clk
 *
 * 本文件把拓展通过 `spiration_agent_api` 注册的 provider（纯 C 函数表）包装成
 * 宿主内部使用的 `spiration::agent::provider` 对象，并在两者之间完成
 * `provider_request` / `provider_response` 与 JSON 的互转：
 *
 * ```
 * agent_registry ── bridge_provider ──▶ provider vtable（拓展实现）
 *                         │
 *                         └── request JSON / response JSON
 * ```
 *
 * 生命周期约定（与 `spiration/api/agent.h` 一致）：
 * - vtable 与 `user_data` 由拓展持有，注销后宿主不再访问；
 * - 宿主在拓展卸载时会自动注销其遗留 provider，但不会释放 `user_data`；
 * - `execute` 与注销通过 `provider_binding::call_mutex` 串行，避免访问
 *   拓展已释放的状态。
 */

#include "internal.h"

#include <spiration/extension/builtin/agent/provider.h>
#include <spiration/extension/builtin/agent/registry.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <tl/optional.hpp>

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace capi {

namespace {

using nlohmann::json;

/** @brief 智能体相关日志的标签。 */
const char* kLogTag = "extension/agent";

/** @brief 记录一条日志（应用尚未就绪时静默丢弃）。 */
void log_agent(logging::level level, const std::string& message) {
    if (std::shared_ptr<logging::logger> out = logger()) {
        out->log({level, message, kLogTag});
    }
}

/** @brief 状态码 → 可读文本。 */
const char* status_name(spiration_status status) {
    switch (status) {
        case SPIRATION_OK: return "ok";
        case SPIRATION_ERR_INVALID_ARGUMENT: return "invalid_argument";
        case SPIRATION_ERR_NOT_FOUND: return "not_found";
        case SPIRATION_ERR_UNSUPPORTED: return "unsupported";
        case SPIRATION_ERR_NOT_READY: return "not_ready";
        case SPIRATION_ERR_ALREADY_EXISTS: return "already_exists";
        case SPIRATION_ERR_IO: return "io";
        case SPIRATION_ERR_INTERNAL: return "internal";
        default: return "unknown";
    }
}

/* -------------------------------------------------------------------------
 * provider 注册表（宿主侧）
 * ---------------------------------------------------------------------- */

/**
 * @brief 一次 provider 注册的宿主侧绑定。
 * @note vtable 为**值拷贝**：拓展侧结构体只要求在注册调用期间有效即可，
 *       即使未来接口追加字段也不影响既有注册。
 */
struct provider_binding {
    /** provider 名称。 */
    std::string name;
    /** 提供者拓展 ID。 */
    std::string owner;
    /** 注册时传入的拓展描述符（仅用于归属清理）。 */
    const spiration_extension_desc* self = nullptr;
    /** 注册时拷贝的 provider 函数表。 */
    spiration_agent_provider vtable;
    /** 拓展自定义上下文。 */
    void* user_data = nullptr;
    /** 缓存的思考挡位。 */
    std::vector<agent::reasoning_level> levels;
    /** 串行化 execute 与注销。 */
    std::mutex call_mutex;
    /** 注销后置为 `false`，宿主不再调用 vtable。 */
    bool active = true;
};

/** @brief 宿主侧 provider 登记簿。 */
struct provider_book {
    std::mutex mutex;
    /** 名称 → 绑定。 */
    std::map<std::string, std::shared_ptr<provider_binding>> by_name;
    /** 拓展 → 其注册的名称列表。 */
    std::map<const spiration_extension_desc*, std::vector<std::string>> by_owner;
};

provider_book& book() {
    static provider_book instance;
    return instance;
}

/** @brief 全部思考挡位（provider 未声明时的默认值）。 */
std::vector<agent::reasoning_level> default_levels() {
    return {agent::reasoning_level::none,   agent::reasoning_level::low,
            agent::reasoning_level::medium, agent::reasoning_level::high,
            agent::reasoning_level::xhigh,  agent::reasoning_level::max};
}

/** @brief C 挡位 → 宿主挡位（越界回退为 `medium`）。 */
agent::reasoning_level to_level(spiration_reasoning_level level) {
    switch (level) {
        case SPIRATION_REASONING_NONE: return agent::reasoning_level::none;
        case SPIRATION_REASONING_LOW: return agent::reasoning_level::low;
        case SPIRATION_REASONING_MEDIUM: return agent::reasoning_level::medium;
        case SPIRATION_REASONING_HIGH: return agent::reasoning_level::high;
        case SPIRATION_REASONING_XHIGH: return agent::reasoning_level::xhigh;
        case SPIRATION_REASONING_MAX: return agent::reasoning_level::max;
        default: return agent::reasoning_level::medium;
    }
}

/**
 * @brief 注册时向拓展查询支持的思考挡位。
 * @return 挡位列表；未声明或查询失败时回退为全部挡位。
 */
std::vector<agent::reasoning_level> query_levels(
    const spiration_agent_provider& vtable, void* user_data) {
    if (vtable.supported_reasoning_levels == nullptr) return default_levels();

    const size_t count =
        vtable.supported_reasoning_levels(user_data, nullptr, static_cast<size_t>(0));
    if (count == 0u) return default_levels();

    /* 上限防护：避免异常的拓展触发巨量分配。 */
    const size_t capacity = std::min<size_t>(count, 64u);
    std::vector<spiration_reasoning_level> raw(capacity);
    const size_t got =
        vtable.supported_reasoning_levels(user_data, raw.data(), capacity);
    const size_t used = std::min<size_t>(got, capacity);

    std::vector<agent::reasoning_level> levels;
    levels.reserve(used);
    for (size_t i = 0; i < used; ++i) levels.push_back(to_level(raw[i]));
    if (levels.empty()) return default_levels();
    return levels;
}

/* -------------------------------------------------------------------------
 * JSON 序列化 / 反序列化
 * ---------------------------------------------------------------------- */

/** @brief 可选文本 → JSON（缺失或空值写 `null`）。 */
json optional_text(const tl::optional<std::string>& value) {
    if (!value.has_value()) return json(nullptr);
    return json(value.value());
}

/** @brief 序列化为单行 JSON（非法 UTF-8 以替换字符落盘，不抛异常）。 */
std::string dump_json(const json& value) {
    return value.dump(-1, ' ', false, json::error_handler_t::replace);
}

/** @brief `provider_request` → 请求 JSON。 */
json request_to_json(const agent::provider_request& req) {
    json messages = json::array();
    for (const agent::chat_message& message : req.messages) {
        json calls = json::array();
        for (const agent::tool_call& call : message.tool_calls) {
            calls.push_back({{"id", call.id},
                             {"function_name", call.function_name},
                             {"arguments", call.arguments}});
        }
        messages.push_back({{"role", message.role},
                            {"content", optional_text(message.content)},
                            {"reasoning_content",
                             optional_text(message.reasoning_content)},
                            {"tool_call_id", optional_text(message.tool_call_id)},
                            {"name", optional_text(message.name)},
                            {"tool_calls", calls}});
    }

    json tools = json::array();
    for (const agent::tool_definition& definition : req.tools) {
        tools.push_back({{"name", definition.function_name},
                         {"description", definition.description},
                         {"parameters", definition.parameters}});
    }

    return json{{"model", req.model},
                {"endpoint", req.endpoint},
                {"api_key", req.api_key},
                {"timeout_seconds", req.timeout_seconds},
                {"max_tokens", req.max_tokens},
                {"temperature", req.temperature},
                {"stream", req.stream},
                {"reasoning", agent::reasoning_level_to_string(req.reasoning)},
                {"messages", messages},
                {"tools", tools}};
}

/** @brief 响应 JSON → `provider_response`。 */
agent::provider_response response_from_json(const json& value) {
    agent::provider_response resp;

    if (value.contains("content") && !value["content"].is_null()) {
        resp.content = value["content"].get<std::string>();
    }
    if (value.contains("reasoning_content") &&
        !value["reasoning_content"].is_null()) {
        resp.reasoning_content = value["reasoning_content"].get<std::string>();
    }
    if (value.contains("tool_calls") && value["tool_calls"].is_array()) {
        for (const json& item : value["tool_calls"]) {
            agent::tool_call call;
            call.id = item.value("id", std::string());
            call.function_name = item.value("function_name", std::string());
            call.arguments = item.value("arguments", std::string());
            if (call.function_name.empty()) continue;
            resp.tool_calls.push_back(std::move(call));
        }
    }
    resp.finish_reason = value.value("finish_reason", std::string());
    resp.prompt_tokens = value.value("prompt_tokens", 0);
    resp.completion_tokens = value.value("completion_tokens", 0);
    return resp;
}

/* -------------------------------------------------------------------------
 * 流式回调
 * ---------------------------------------------------------------------- */

/** @brief 正文增量回调。`user_data` 为 `provider_context*`。 */
void SPIRATION_CALL delta_trampoline(void* user_data,
                                     spiration_str_view delta) {
    agent::provider_context* ctx =
        static_cast<agent::provider_context*>(user_data);
    if (ctx == nullptr || !ctx->on_delta) return;
    ctx->on_delta(text_of(delta));
}

/** @brief 思考过程增量回调。`user_data` 为 `provider_context*`。 */
void SPIRATION_CALL reasoning_trampoline(void* user_data,
                                         spiration_str_view delta) {
    agent::provider_context* ctx =
        static_cast<agent::provider_context*>(user_data);
    if (ctx == nullptr || !ctx->on_reasoning_delta) return;
    ctx->on_reasoning_delta(text_of(delta));
}

/** @brief 中止检查回调。`user_data` 为 `provider_context*`。 */
spiration_bool SPIRATION_CALL stop_trampoline(void* user_data) {
    agent::provider_context* ctx =
        static_cast<agent::provider_context*>(user_data);
    if (ctx == nullptr || !ctx->should_stop) return SPIRATION_FALSE;
    return ctx->should_stop() ? SPIRATION_TRUE : SPIRATION_FALSE;
}

/* -------------------------------------------------------------------------
 * 桥接 provider
 * ---------------------------------------------------------------------- */

/**
 * @brief 把 C provider 函数表适配为宿主 `agent::provider`。
 * @note 每次 `create_provider` 会构造一个新实例，但共享同一份绑定，
 *       因此注销对所有实例即时生效。
 */
class bridge_provider : public agent::provider {
public:
    explicit bridge_provider(std::shared_ptr<provider_binding> binding)
        : binding_(std::move(binding)) {}

    std::string name() const override { return binding_->name; }

    std::vector<agent::reasoning_level> supported_reasoning_levels()
        const override {
        return binding_->levels;
    }

    std::unique_ptr<agent::provider_context> build_context(
        const agent::provider_request& req) const override {
        /* 请求规格由 `provider_context` 自身持有；序列化推迟到 execute，
           使宿主侧看到的是最终请求。 */
        return std::unique_ptr<agent::provider_context>(
            new agent::provider_context(req));
    }

    agent::provider_response execute(
        agent::provider_context& ctx) const override {
        std::lock_guard<std::mutex> lock(binding_->call_mutex);
        if (!binding_->active || binding_->vtable.execute == nullptr) return {};

        std::string request_text;
        try {
            request_text = dump_json(request_to_json(ctx.request()));
        } catch (const std::exception& e) {
            log_agent(logging::level::error,
                      fmt::format("provider \"{}\": serialize request failed: {}",
                                  binding_->name, e.what()));
            return {};
        }
        log_agent(logging::level::info,
                  fmt::format("provider \"{}\": executing model \"{}\" (stream={})",
                              binding_->name, ctx.request().model,
                              ctx.request().stream ? 1 : 0));

        spiration_agent_stream stream;
        std::memset(&stream, 0, sizeof(stream));
        stream.on_delta = &delta_trampoline;
        stream.on_reasoning_delta = &reasoning_trampoline;
        stream.should_stop = &stop_trampoline;
        stream.user_data = &ctx;

        spiration_str_view response = spiration_str_view_of(nullptr, 0u);
        const spiration_status status = binding_->vtable.execute(
            binding_->user_data, view_of(request_text), &stream, &response);
        if (status != SPIRATION_OK) {
            log_agent(logging::level::error,
                      fmt::format("provider \"{}\": execute failed ({})",
                                  binding_->name, status_name(status)));
            return {};
        }

        try {
            agent::provider_response parsed =
                response_from_json(json::parse(text_of(response)));
            log_agent(
                logging::level::info,
                fmt::format(
                    "provider \"{}\": finished (content={}, tool_calls={}, tokens={}/{})",
                    binding_->name,
                    parsed.content.has_value() ? parsed.content.value().size()
                                               : 0u,
                    parsed.tool_calls.size(), parsed.prompt_tokens,
                    parsed.completion_tokens));
            return parsed;
        } catch (const std::exception& e) {
            log_agent(logging::level::error,
                      fmt::format("provider \"{}\": parse response failed: {}",
                                  binding_->name, e.what()));
            return {};
        }
    }

private:
    std::shared_ptr<provider_binding> binding_;
};

/* -------------------------------------------------------------------------
 * 注册 / 注销
 * ---------------------------------------------------------------------- */

/** @brief 从登记簿中移除名称，并同步维护归属索引。 */
void erase_binding(const std::shared_ptr<provider_binding>& binding) {
    provider_book& registry = book();
    registry.by_name.erase(binding->name);

    std::map<const spiration_extension_desc*, std::vector<std::string>>::iterator
        owner = registry.by_owner.find(binding->self);
    if (owner == registry.by_owner.end()) return;
    std::vector<std::string>& names = owner->second;
    names.erase(std::remove(names.begin(), names.end(), binding->name),
                names.end());
    if (names.empty()) registry.by_owner.erase(owner);
}

/**
 * @brief 注销一个 provider 绑定。
 * @param name 名称。
 * @param self 调用方描述符；`nullptr` 表示强制注销（拓展卸载清理）。
 */
spiration_status unregister_bound(const std::string& name,
                                  const spiration_extension_desc* self) {
    std::shared_ptr<provider_binding> binding;
    {
        std::lock_guard<std::mutex> lock(book().mutex);
        std::map<std::string, std::shared_ptr<provider_binding>>::iterator it =
            book().by_name.find(name);
        if (it == book().by_name.end()) return SPIRATION_OK; /* 幂等 */
        binding = it->second;
        if (self != nullptr && binding->self != self) {
            return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                        "unregister_provider: provider is owned by another "
                        "extension");
        }
        erase_binding(binding);
    }

    /* 先从注册表移除，此后 create_provider 不再返回该 provider；
       再等待正在进行的 execute 结束，最后标记为失效。 */
    agent::agent_registry::instance().unregister_provider(name);
    {
        std::lock_guard<std::mutex> lock(binding->call_mutex);
        binding->active = false;
    }
    log_agent(logging::level::info,
              fmt::format("unregistered provider \"{}\"", name));
    return SPIRATION_OK;
}

/* -------------------------------------------------------------------------
 * 智能体接口条目
 * ---------------------------------------------------------------------- */

spiration_status SPIRATION_CALL agent_register_provider(
    const spiration_extension_desc* self, spiration_str_view name_view,
    const spiration_agent_provider* provider, void* user_data) {
    if (self == nullptr || name_view.data == nullptr || name_view.length == 0u) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_provider: empty name or descriptor");
    }
    if (provider == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_provider: null provider");
    }

    /* 只要求覆盖到 execute 字段：允许旧版拓展使用更小的结构体。 */
    const size_t required = offsetof(spiration_agent_provider, reserved);
    if (provider->size < required ||
        SPIRATION_API_VERSION_MAJOR_OF(provider->version) !=
            SPIRATION_API_VERSION_MAJOR_OF(SPIRATION_STRUCT_VERSION)) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_provider: incompatible provider table");
    }
    if (provider->execute == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_provider: missing execute callback");
    }

    spiration_agent_provider vtable;
    std::memset(&vtable, 0, sizeof(vtable));
    std::memcpy(&vtable, provider,
                std::min<size_t>(provider->size, sizeof(vtable)));
    vtable.size = static_cast<uint32_t>(sizeof(vtable));

    const std::string name = text_of(name_view);
    if (agent::agent_registry::instance().has_provider(name)) {
        return fail(SPIRATION_ERR_ALREADY_EXISTS,
                    "register_provider: name already in use: " + name);
    }

    /* 在登记簿锁之外调用拓展：查询挡位可能回跳进脚本运行时。 */
    std::shared_ptr<provider_binding> binding(new provider_binding());
    binding->name = name;
    binding->owner = text_of(self->id);
    binding->self = self;
    binding->vtable = vtable;
    binding->user_data = user_data;
    try {
        binding->levels = query_levels(vtable, user_data);
    } catch (const std::exception& e) {
        return fail(SPIRATION_ERR_INTERNAL,
                    std::string("register_provider: query levels failed: ") +
                        e.what());
    }

    {
        std::lock_guard<std::mutex> lock(book().mutex);
        if (book().by_name.find(name) != book().by_name.end()) {
            return fail(SPIRATION_ERR_ALREADY_EXISTS,
                        "register_provider: name already in use: " + name);
        }
        book().by_name[name] = binding;
        book().by_owner[self].push_back(name);
    }

    try {
        agent::agent_registry::instance().register_provider(
            name, [binding]() -> std::unique_ptr<agent::provider> {
                return std::unique_ptr<agent::provider>(
                    new bridge_provider(binding));
            });
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(book().mutex);
        erase_binding(binding);
        return fail(SPIRATION_ERR_INTERNAL,
                    std::string("register_provider: registry failed: ") +
                        e.what());
    }

    log_agent(logging::level::info,
              fmt::format("registered provider \"{}\" from \"{}\"",
                          name, binding->owner));
    return SPIRATION_OK;
}

spiration_status SPIRATION_CALL agent_unregister_provider(
    const spiration_extension_desc* self, spiration_str_view name_view) {
    if (name_view.data == nullptr || name_view.length == 0u) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "unregister_provider: empty name");
    }
    return unregister_bound(text_of(name_view), self);
}

/** @brief 组装智能体函数表。 */
spiration_agent_api make_agent_api() {
    spiration_agent_api api;
    std::memset(&api, 0, sizeof(api));
    api.version = SPIRATION_AGENT_VERSION;
    api.size = static_cast<uint32_t>(sizeof(api));
    api.register_provider = agent_register_provider;
    api.unregister_provider = agent_unregister_provider;
    return api;
}

} // namespace

const spiration_agent_api* agent_interface(uint32_t required_version) {
    /* 主版本必须一致，且宿主实现不得低于请求的次版本。 */
    if (SPIRATION_API_VERSION_MAJOR_OF(required_version) !=
        SPIRATION_API_VERSION_MAJOR_OF(SPIRATION_AGENT_VERSION)) {
        return NULL;
    }
    if (required_version > SPIRATION_AGENT_VERSION) return NULL;

    static const spiration_agent_api api = make_agent_api();
    return &api;
}

void release_agent_owned(const spiration_extension_desc* desc) {
    if (desc == nullptr) return;

    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(book().mutex);
        std::map<const spiration_extension_desc*,
                 std::vector<std::string>>::iterator it =
            book().by_owner.find(desc);
        if (it == book().by_owner.end()) return;
        names = it->second;
    }

    for (size_t i = 0; i < names.size(); ++i) {
        unregister_bound(names[i], nullptr);
    }

    /* 兜底：若上面因异常未清干净，仍移除归属条目，避免悬挂描述符指针。 */
    std::lock_guard<std::mutex> lock(book().mutex);
    book().by_owner.erase(desc);
}

} // namespace capi
} // namespace spiration
