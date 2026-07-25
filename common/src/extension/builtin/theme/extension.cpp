/**
 * @file extension.cpp
 * @brief 拓展入口。
 * @author clk
 */

#include <extension/builtin/theme/extension.h>
#include <extension/extension_api.h>
#include <ui/theme_manager.h>
#include <utils/console.h>

namespace spiration {
namespace theme {

bool extension::initialize() {
    api_->register_theme_profile("dark");
    api_->log_info("initialized (active: %s)", spiration::theme_manager::active().c_str());
    return true;
}

void extension::shutdown() {
}

} // namespace theme
} // namespace spiration
