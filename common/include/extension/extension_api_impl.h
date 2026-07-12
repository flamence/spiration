/**
 * @file extension_api_impl.h
 * @brief 扩展 API 上下文的默认实现。
 * @author clk
 */

#pragma once

#include <extension/extension_api.h>
#include <memory>

namespace spiration {

class window;
class root;

/**
 * @brief extension_api 的默认实现。
 */
class extension_api_impl : public extension_api {
public:
    explicit extension_api_impl(std::shared_ptr<window> win);

    std::shared_ptr<renderer> get_renderer() const override;
    std::shared_ptr<window> get_window() const override;

    std::string tr(const std::string& key) const override;
    std::string tr(const std::string& key,
                   const std::vector<std::string>& args) const override;

    void log_info(const std::string& message) const override;
    void log_warning(const std::string& message) const override;
    void log_error(const std::string& message) const override;

    std::string app_data_dir() const override;
    std::string extension_data_dir(const std::string& extension_id) const override;

    void request_repaint() const override;

    void add_menu_item(const std::string& menu_name,
                       const std::string& label,
                       std::function<void()> callback) override;

    void open_tab(std::unique_ptr<tab> t) override;

    void register_theme_profile(const std::string& name) override;
    void set_theme_param(const std::string& profile,
                         const std::string& key,
                         const color& value) override;

    /**
     * @brief 设置根控件引用。
     */
    void set_root(root* r) { root_ = r; }

private:
    std::shared_ptr<window> window_;
    root* root_ = nullptr;
};

} // namespace spiration
