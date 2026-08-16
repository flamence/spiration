#include <spiration/extension/builtin/theme/extension.h>

#include <format>

namespace spiration {
namespace theme {

bool extension::initialize() {
    api->register_theme_profile("dark");
    api->log_info(std::format("initialized (active: {})", spiration::theme_manager::active()));
    return true;
}

void extension::shutdown() {
}

} // namespace theme
} // namespace spiration
