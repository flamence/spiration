/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <spiration/application.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension.hpp>

namespace spiration {
namespace i18n {

/**
 * @brief 内置拓展国际化。
 * @note 仅用于提供与国际化管理器的交互。
 */
class extension : public spiration::extension {
public:
    extension() : spiration::extension({
        "com.flamence.spiration.i18n",
        "0.2"
    }) {}

    tl::optional<std::string> name() const override { return api->tr("i18n:name"); }

    tl::optional<std::string> description() const override { return api->tr("i18n:description"); }

    init_phase phase() const override { return init_phase::early; }

    bool initialize() override;

    void shutdown() override;
};

} // namespace i18n
} // namespace spiration
