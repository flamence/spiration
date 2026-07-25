/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <extension/builtin/edit/edit_tab.h>
#include <extension/extension.h>
#include <utils/i18n.h>
#include <string>

namespace spiration {
namespace theme {

/**
 * @brief 内置拓展主题。
 */
class extension : public spiration::extension {
public:
    std::string id() const override          { return "com.flamence.spiration.theme"; }
    std::string name() const override        { return i18n_manager::tr("extension.theme.name"); }
    std::string version() const override     { return "0.1"; }
    std::string description() const override { return i18n_manager::tr("extension.theme.description"); }

    bool initialize() override;
    void shutdown() override;
};

} // namespace theme
} // namespace spiration
