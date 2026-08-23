/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <spiration/extension/extension.hpp>

namespace spiration {
namespace theme {

/**
 * @brief 内置拓展主题。
 */
class extension : public spiration::extension {
public:
    extension() : spiration::extension({
        "com.flamence.spiration.theme",
        "0.2"
    }) {}

    tl::optional<std::string> name() const override { return api->tr("theme:name"); }
    tl::optional<std::string> description() const override { return api->tr("theme:description"); }
    bool initialize() override;
    void shutdown() override;
};

} // namespace theme
} // namespace spiration
