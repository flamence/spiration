/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <extension/extension.h>
#include <utils/i18n.h>

namespace spiration {
namespace i18n {

/**
 * @brief 内置拓展国际化。
 */
class extension : public spiration::extension {
public:
    std::string id() const override          { return "com.flamence.spiration.i18n"; }
    std::string name() const override        { return i18n_manager::tr("extension.i18n.name"); }
    std::string version() const override     { return "0.1"; }
    std::string description() const override { return i18n_manager::tr("extension.i18n.description"); }

    bool initialize() override;
    void shutdown() override;
};

} // namespace i18n
} // namespace spiration
