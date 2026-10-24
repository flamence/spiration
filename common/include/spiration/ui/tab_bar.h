/**
 * @file tab_bar.h
 * @brief 标签栏与标签控件，支持可滚动的标签头、渐显隐指示条与浏览器式快捷键。
 * @author clk
 */

#pragma once

#include <spiration/export.h>
#include <spiration/ui/container.h>
#include <spiration/ui/scroll_row.h>
#include <spiration/utils/animation.h>
#include <spiration/window/event.h>
#include <spiration/window/virtual_key.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

class tab_bar;
class tab_head_item;
class button;

/**
 * @brief 单个标签页。
 */
class SPIRATION_EXPORT tab : public container {
    friend class tab_bar;
public:
    tab() = default;

    // 不可拷贝（基类已删除拷贝）。
    tab(const tab&) = delete;
    tab& operator=(const tab&) = delete;

    void set_title(const std::string& t) { 
        title_ = t; 
        if (on_title_change_) on_title_change_(title_);
    }
    const std::string& title() const { return title_; }

    bool is_active() const { return active_; }

    void set_on_title_change(std::function<void(const std::string&)> cb) {
        on_title_change_ = std::move(cb);
    }

    virtual void on_activate() {}
    virtual void on_deactivate() {}

    void paint(std::shared_ptr<renderer> renderer) override = 0;

protected:
    std::string title_;
    bool active_ = false;
    std::function<void(const std::string&)> on_title_change_;
};

/**
 * @brief 标签栏，管理多个 tab，显示可滚动的标签头并切换内容区域。
 *
 * 快捷键配置独立于平台实现：`primary` 表示平台主修饰键
 * （macOS 为 Command，其余为 Ctrl），`ctrl_only` 用于 Ctrl+Tab 这类
 * 在 macOS 上仍使用物理 Ctrl 的浏览器惯例。
 */
class tab_bar : public container {
public:
    /// @brief 单个快捷键：key_code 为 0 表示禁用。
    struct tab_shortcut {
        int key_code = 0;
        bool shift = false;
        bool primary = true;   ///< 使用平台主修饰键（macOS Cmd / 其余 Ctrl）。
        bool ctrl_only = false;///< 强制物理 Ctrl（如 Ctrl+Tab）。
        bool alt = false;
    };

    /// @brief 可配置的标签快捷键表，后续可由设置界面替换。
    struct tab_shortcuts {
        tab_shortcut close{vk::W};
        tab_shortcut new_tab{vk::T};
        tab_shortcut next{vk::Tab, false, false, true};
        tab_shortcut prev{vk::Tab, true, false, true};
        /// @brief 主修饰键 + 1..8 选择第 N 个；+9 选择最后一个。
        bool number_keys = true;
    };

    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;
    widget* hit_test_hover(float x, float y) const override;

    void set_repaint_callback(std::function<void()> cb) override;
    void set_window_action_callback(std::function<void(int)> cb) override;

    void add_tab(std::unique_ptr<tab> t);
    void activate_tab(int index);
    void close_tab(int index);
    int tab_count() const { return static_cast<int>(tabs_.size()); }
    int active_index() const { return active_index_; }
    tab* get_tab(int i) {
        if (i < 0 || i >= static_cast<int>(tabs_.size())) return nullptr;
        return tabs_[i].get();
    }

    /**
     * @brief 设置“新建标签”回调；设置后标签栏右侧显示“+”。
     */
    void set_new_tab_callback(std::function<void()> cb);

    /**
     * @brief 在“+”左侧追加头部动作控件（如历史对话）。
     */
    void add_header_action(std::unique_ptr<widget> w);

    /// @brief 移除全部拓展添加的头部动作（保留“+”），供拓展卸载时清理。
    void clear_header_actions();

    /**
     * @brief 标签头右键菜单回调，参数为标签索引与窗口坐标。
     */
    void set_on_tab_context_menu(std::function<void(int, float, float)> cb) {
        on_tab_context_menu_ = std::move(cb);
    }

    void set_shortcuts(const tab_shortcuts& s) { shortcuts_ = s; }
    const tab_shortcuts& shortcuts() const { return shortcuts_; }

    /**
     * @brief 处理全局标签快捷键。
     * @return true 表示已消费该按键。
     */
    bool handle_shortcut(const key_event_data& key);

private:
    std::vector<std::unique_ptr<tab>> tabs_;
    int active_index_ = -1;

    std::unique_ptr<scroll_row> header_row_;
    std::vector<tab_head_item*> tab_heads_;

    /// @brief 已关闭但需等事件处理结束再销毁的标签页/标签头（避免 delete-this）。
    std::vector<std::unique_ptr<tab>> pending_close_tabs_;
    std::vector<std::unique_ptr<widget>> pending_close_heads_;

    std::unique_ptr<container> actions_;
    button* new_btn_ = nullptr;
    std::function<void()> on_new_tab_;
    std::function<void(int, float, float)> on_tab_context_menu_;
    tab_shortcuts shortcuts_;

    /**
     * @brief 底部指示条渐显隐。
     */
    float curr_indicator_pos_ = 0.0f;
    float curr_indicator_width_ = 0.0f;
    color_transition curr_indicator_alpha_{color::transparent()};

    /// @brief 切换标签时内容区淡入。
    value_transition content_fade_{1.0f};

    float prev_indicator_pos_ = 0.0f;
    float prev_indicator_width_ = 0.0f;
    color_transition prev_indicator_alpha_{color::transparent()};

    void start_indicator_fade(float new_pos, float new_width);
    void apply_active_state(int index);
    void apply_callbacks(widget& w);
    void layout_actions();
    float actions_width() const;
    int index_of_head(const tab_head_item* head) const;
    /// @brief 销毁已关闭的标签页；只在事件处理之外调用。
    void reap_closed_tabs();

    static constexpr float TAB_HEADER_H = 30.0f;
    static constexpr float TAB_FIXED_W = 120.0f;
    static constexpr float HEADER_ACTION_SIZE = 28.0f;
    static constexpr float HEADER_ACTION_GAP = 2.0f;
    static constexpr float HEADER_ACTION_PAD = 4.0f;
};

} 
