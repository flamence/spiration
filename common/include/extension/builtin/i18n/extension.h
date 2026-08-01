/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <application.h>
#include <extension/builtin/i18n/i18n.h>
#include <extension/extension.h>

namespace spiration {
namespace i18n {

/**
 * @brief 内置拓展国际化。
 */
class extension : public spiration::extension {
public:
    std::string id() const override          { return ID; }
    std::string name() const override        { return api->tr("extension.i18n.name"); }
    std::string version() const override     { return "0.2"; }
    std::string description() const override { return api->tr("extension.i18n.description"); }
    init_phase phase() const override        { return init_phase::early; }

    bool initialize() override;
    void shutdown() override;

    static inline std::string ID = "com.flamence.spiration.i18n";
};

} // namespace i18n
} // namespace spiration
