/**
 * @file extension.cpp
 * @brief 拓展入口。
 * @author clk
 */

#include <extension/builtin/i18n/extension.h>
#include <extension/extension_api.h>
#include <utils/i18n.h>
#include <utils/platform.h>
#include <string>

namespace spiration {
namespace i18n {

bool extension::initialize() {
    return true;
}

void extension::shutdown() {
    api_->log_info("shutdown");
}

} // namespace i18n
} // namespace spiration
