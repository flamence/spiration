/**
 * @file extension_api.h
 * @brief 扩展 API 上下文，提供扩展可用的平台无关接口。
 * @author clk
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <ui/color.h>

namespace spiration {

class renderer;
class window;
class tab;
class root;

/**
 * @brief 扩展 API 上下文。
 *
 * 扩展在 initialize() 时获得此上下文引用，通过它访问宿主应用功能。
 * 所有接口均为平台无关，确保扩展跨平台兼容。
 */
class extension_api {
public:
    explicit extension_api(std::shared_ptr<window> win);

    /**
     * @brief 设置根控件引用。
     */
    void set_root(root* r) { root_ = r; }

    /**
     * @brief 获取渲染器实例。
     */
    std::shared_ptr<renderer> get_renderer() const;

    /**
     * @brief 获取窗口实例。
     */
    std::shared_ptr<window> get_window() const;

    /**
     * @brief 获取翻译文本。
     * @param key 翻译键
     * @return 翻译后的字符串
     */
    std::string tr(const std::string& key) const;

    /**
     * @brief 获取带参数的翻译文本。
     * @param key 翻译键
     * @param args 参数列表
     * @return 翻译后的字符串
     */
    std::string tr(const std::string& key,
                   const std::vector<std::string>& args) const;

    /**
     * @brief 记录日志信息。
     */
    void log_info(const std::string& message) const;

    /**
     * @brief 记录日志警告。
     */
    void log_warning(const std::string& message) const;

    /**
     * @brief 记录日志错误。
     */
    void log_error(const std::string& message) const;

    /**
     * @brief 获取应用数据目录。
     */
    std::string app_data_dir() const;

    /**
     * @brief 获取扩展数据目录。
     */
    std::string extension_data_dir(const std::string& extension_id) const;

    /**
     * @brief 请求重绘窗口。
     */
    void request_repaint() const;

    /**
     * @brief 在指定菜单中添加子项。
     * @param menu_name 菜单标题
     * @param label 子项标签
     * @param callback 点击回调
     */
    void add_menu_item(const std::string& menu_name,
                       const std::string& label,
                       std::function<void()> callback);

    /**
     * @brief 在标签栏中打开一个新标签页。
     * @param t 标签页实例
     */
    void open_tab(std::unique_ptr<tab> t);

    /**
     * @brief 注册新主题 profile。
     * @param name profile 名称
     */
    void register_theme_profile(const std::string& name);

    /**
     * @brief 为指定 profile 设置主题参数。
     * @param profile profile 名称
     * @param key 参数键名
     * @param value 颜色值
     */
    void set_theme_param(const std::string& profile,
                         const std::string& key,
                         const color& value);

private:
    std::shared_ptr<window> window_;
    root* root_ = nullptr;
};

} // namespace spiration
