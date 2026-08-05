/**
 * @file sleep_tool.cpp
 * @brief 休眠工具实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/sleep_tool.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <exception>
#include <thread>

namespace spiration {
namespace agent {

std::string sleep_tool::description() const {
    return "Sleep for the given number of seconds. "
           "Use it to wait for background work such as a build or a command "
           "started in a terminal before reading its output. "
           "The wait is cancellable when the user stops generation.";
}

std::string sleep_tool::parameters_json() const {
    return R"({
    "type": "object",
    "properties": {
        "seconds": {
            "type": "number",
            "description": "How long to sleep, in seconds (may be fractional). Default 1."
        }
    }
})";
}

std::string sleep_tool::execute(const std::string& args_json) {
    double seconds = 1.0;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        seconds = j.value("seconds", 1.0);
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (seconds < 0.0) seconds = 0.0;
    if (seconds > 3600.0) seconds = 3600.0;

    const auto step = std::chrono::milliseconds(100);
    const auto total = std::chrono::milliseconds(static_cast<long long>(seconds * 1000.0));
    for (auto waited = std::chrono::milliseconds(0); waited < total; waited += step) {
        if (should_stop && should_stop())
            return "[cancelled]";
        std::this_thread::sleep_for(step);
    }
    if (should_stop && should_stop())
        return "[cancelled]";

    console::info("extension/agent/sleep", "slept %.1fs", seconds);
    return "ok";
}

} // namespace agent
} // namespace spiration
