#include <extension/builtin/theme/extension.h>

namespace spiration {
namespace theme {

bool extension::initialize() {
    api->register_theme_profile("dark");
    api->log_info("initialized (active: %s)", spiration::theme_manager::active().c_str());
    return true;
}

void extension::shutdown() {
}

} // namespace theme
} // namespace spiration
