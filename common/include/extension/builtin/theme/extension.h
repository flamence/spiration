/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <application.h>
#include <extension/builtin/edit/edit_tab.h>
#include <extension/builtin/i18n/i18n.h>
#include <extension/extension.h>
#include <string>

namespace spiration {
namespace theme {

/**
 * @brief 内置拓展主题。
 */
class extension : public spiration::extension {
public:
    std::string id() const override          { return "com.flamence.spiration.theme"; }
    std::string name() const override        { return api->tr("extension.theme.name"); }
    std::string version() const override     { return "0.2"; }
    std::string description() const override { return api->tr("extension.theme.description"); }

    bool initialize() override;
    void shutdown() override;
};

} // namespace theme
} // namespace spiration
