/**
 * @file extension.cpp
 * @brief 拓展基类实现。
 * @author clk
 */

#include <extension/extension.h>
#include <extension/extension_manager.h>

namespace spiration {

void extension::register_service_impl(const std::string& name, void* ptr) {
    extension_manager::register_service(id(), name, ptr);
}

void* extension::get_service_impl(const std::string& name) const {
    return extension_manager::get_service(id(), name);
}

}
