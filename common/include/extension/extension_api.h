/**
 * @file extension_api.h
 * @brief 拓展 API 上下文。
 * @author clk
 */

#pragma once

#include <extension/extension_manager.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <ui/color.h>

namespace spiration {

class renderer;
class window;
class tab;
class extension;

/**
 * @brief 拓展 API 上下文。
 */
class extension_api {
public:
    explicit extension_api(std::string id);

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
     * @brief 记录格式化日志信息。
     * @param fmt 格式化消息
     */
    void log_info(const char* fmt, ...) const;

    /**
     * @brief 记录格式化日志警告。
     * @param fmt 格式化消息
     */
    void log_warning(const char* fmt, ...) const;

    /**
     * @brief 记录格式化日志错误。
     * @param fmt 格式化消息
     */
    void log_error(const char* fmt, ...) const;

    /**
     * @brief 获取应用数据目录。
     */
    std::string app_data_dir() const;

    /**
     * @brief 获取扩展数据目录。
     */
    std::string extension_data_dir(const std::string& extension_id) const;

    /**
     * @brief 获取本扩展所在的目录路径。
     */
    std::string extension_dir() const;

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
     * @brief 若指定标签页实例仍存在则激活它。
     * @param t 标签页实例
     * @return true 已找到并激活；false 标签页不存在
     */
    bool activate_tab(tab* t);

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

    /**
     * @brief 根据 ID 查找已加载的扩展。
     * @param id 扩展 ID
     * @return 扩展指针，未找到返回 nullptr
     */
    extension* get_extension(const std::string& id) const;

    /**
     * @brief 获取所有已加载的扩展。
     */
    std::vector<extension*> get_extensions() const;

    /**
     * @brief 订阅事件。
     * @param event 事件名称
     * @param callback 回调函数
     * @return 订阅 ID，用于 off_event 取消
     */
    int on_event(const std::string& event,
                 std::function<void(const std::string& data)> callback) const;

    /**
     * @brief 取消事件订阅。
     * @param subscription_id on_event 返回的 ID
     */
    void off_event(int subscription_id) const;

    /**
     * @brief 发布事件。
     * @param event 事件名称
     * @param data 事件数据
     */
    void emit_event(const std::string& event, const std::string& data) const;

    // ---- 拓展间服务查询 ----

    /**
     * @brief 查询其他拓展暴露的命名服务。
     * @tparam T 期望的服务类型
     * @param extension_id 目标拓展 ID
     * @param name         服务名称
     * @return 服务指针，不存在返回 nullptr
     */
    template<typename T>
    T* get_service(const std::string& extension_id,
                   const std::string& name) const {
        return static_cast<T*>(
            extension_manager::get_service(extension_id, name));
    }

    /** @brief 扩展被加载事件名 */
    static constexpr const char* EVENT_EXTENSION_LOADED = "extension:loaded";
    /** @brief 扩展被卸载事件名 */
    static constexpr const char* EVENT_EXTENSION_UNLOADED = "extension:unloaded";
    /** @brief 标签页打开事件名 */
    static constexpr const char* EVENT_TAB_OPENED = "tab:opened";
    /** @brief 标签页关闭事件名 */
    static constexpr const char* EVENT_TAB_CLOSED = "tab:closed";
    /** @brief 主题切换事件名 */
    static constexpr const char* EVENT_THEME_CHANGED = "theme:changed";

private:
    /** @brief 当前调用方扩展 ID。 */
    std::string id_;

    std::string id() const;
    std::string simple_id() const;
};

} // namespace spiration
