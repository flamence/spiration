/**
 * @file extension.h
 * @author clk
 */

#pragma once

#include <memory>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension_manager.h>
#include <spiration/extension/extension_manifest.h>
#include <spiration/extension/extension_api.h>
#include <spiration/export.h>
#include <spiration/init_phase.h>
#include <string>
#include <tl/optional.hpp>

namespace spiration {

class extension_api;
class extension_manager;

/**
 * @brief 拓展基类。
 */
class SPIRATION_EXPORT extension {
public:
    explicit extension(const manifest_data& data) : manifest_(data) {}
    
    virtual ~extension() = default;

    /**
     * @brief 唯一标识符。
     */
    std::string id() const { return manifest_.id; }

    /**
     * @brief 显示名称。
     */
    virtual tl::optional<std::string> name() const { return tl::nullopt; }

    /**
     * @brief 版本号。
     */
    std::string version() const { return manifest_.version; }

    /**
     * @brief 描述。
     */
    virtual tl::optional<std::string> description() const { return tl::nullopt; }

    /**
     * @brief 声明应在何时被初始化。
     */
    virtual init_phase phase() const { return init_phase::normal; }

    /**
     * @brief 初始化。
     * @return 成功返回 `true` ，否则 `false`。
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭。
     */
    virtual void shutdown() = 0;

protected:
    std::unique_ptr<extension_api> api = nullptr;

private:
    friend extension_manager;

    manifest_data manifest_;
    
    void set_api(std::unique_ptr<extension_api> new_api) { api = std::move(new_api); }
};

}
