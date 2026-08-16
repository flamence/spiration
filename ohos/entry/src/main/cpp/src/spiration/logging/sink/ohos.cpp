/**
 * @file ohos.cpp
 * @brief 终端接收器实现。
 * @author clk
 */

#include <hilog/log.h>
#include <spiration/logging/ohos_sink.h>

namespace spiration {
namespace logging {

void ohos_sink::write(const log& message, log_formatter _) {
    LogLevel ohos_level;
    switch (level) {
        case level::trace:   return;
        case level::debug:   ohos_level = LOG_DEBUG; break;
        case level::info:    ohos_level = LOG_INFO;  break;
        case level::warning: ohos_level = LOG_WARN;  break;
        case level::error:   ohos_level = LOG_ERROR; break;
    }
    OH_LOG_VPrint(LOG_APP, ohos_level, 0x0000, message.tag, ,message.message..c_str());
}

}
}
