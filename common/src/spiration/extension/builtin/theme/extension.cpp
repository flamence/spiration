#include <spiration/extension/builtin/theme/extension.h>
#include <spiration/ui/theme_manager.h>

#include <fmt/format.h>

namespace spiration {
namespace theme {

bool extension::initialize() {
    api->register_theme_profile("dark");
    api->log_info(fmt::format("initialized (active: {})", spiration::theme_manager::active()));
    return true;
}

void extension::shutdown() {
}

} // namespace theme
} // namespace spiration
