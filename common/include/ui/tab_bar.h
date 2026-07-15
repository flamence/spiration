/**
 * @file tab_bar.h
 * @brief 标签栏与标签控件，支持可滚动的标签头、渐显隐指示条。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/scroll_row.h>
#include <utils/animation.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

class tab_bar;

/**
 * @brief 单个标签页。
 */
class tab : public container {
    friend class tab_bar;
public:
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
 * @brief 标签头项，代表单个 tab 在标签栏中的标题按钮。
 */
class tab_head_item : public widget {
    friend class tab_bar;
public:
    explicit tab_head_item(const std::string& title);

    void set_on_activate(std::function<void()> cb) { on_activate_ = std::move(cb); }
    void set_on_close(std::function<void()> cb) { on_close_ = std::move(cb); }

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;

    void set_active(bool active) { active_ = active; }
    bool is_active() const { return active_; }
    void set_title(const std::string& t) { title_ = t; }
    const std::string& get_title() const { return title_; }

private:
    std::string title_;
    bool active_ = false;
    bool hovering_ = false;
    bool close_hovering_ = false;

    color_transition bg_;
    color_transition text_;
    color_transition close_fg_;

    std::function<void()> on_activate_;
    std::function<void()> on_close_;

    void sync_colors();

    static constexpr float CLOSE_BTN_W = 20.0f;
    static constexpr float CLOSE_ICON_SIZE = 10.0f;
};

/**
 * @brief 标签栏，管理多个 tab，显示可滚动的标签头并切换内容区域。
 */
class tab_bar : public container {
public:
    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;

    void add_tab(std::unique_ptr<tab> t);
    void activate_tab(int index);
    void close_tab(int index);
    int tab_count() const { return static_cast<int>(tabs_.size()); }
    int active_index() const { return active_index_; }
    tab* get_tab(int i) {
        if (i < 0 || i >= static_cast<int>(tabs_.size())) return nullptr;
        return tabs_[i].get();
    }

private:
    std::vector<std::unique_ptr<tab>> tabs_;
    int active_index_ = -1;

    std::unique_ptr<scroll_row> header_row_;
    std::vector<tab_head_item*> tab_heads_;

    /**
     * @brief 底部指示条渐显隐。
     */
    float curr_indicator_pos_ = 0.0f;
    float curr_indicator_width_ = 0.0f;
    color_transition curr_indicator_alpha_{color::transparent()};

    float prev_indicator_pos_ = 0.0f;
    float prev_indicator_width_ = 0.0f;
    color_transition prev_indicator_alpha_{color::transparent()};

    void start_indicator_fade(float new_pos, float new_width);

    static constexpr float TAB_HEADER_H = 30.0f;
    static constexpr float TAB_FIXED_W = 120.0f;
};

} 
