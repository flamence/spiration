/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <extension/extension.h>
#include <ui/theme_manager.h>

namespace spiration {
namespace theme {

/**
 * @brief 内置拓展主题。
 */
class extension : public spiration::extension {
public:
    std::string id() const override          { return ID; }
    std::string name() const override        { return api->tr("extension.theme.name"); }
    std::string version() const override     { return "0.2"; }
    std::string description() const override { return api->tr("extension.theme.description"); }

    bool initialize() override;
    void shutdown() override;

    static inline std::string ID = "com.flamence.spiration.theme";
};

} // namespace theme
} // namespace spiration
