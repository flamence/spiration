/**
 * @file tab_bar.h
 * @brief 标签栏与标签控件，支持可滚动的标签头、渐显隐指示条。
 * @author clk
 */

#pragma once

#include <spiration/export.h>
#include <spiration/ui/container.h>
#include <spiration/ui/scroll_row.h>
#include <spiration/utils/animation.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

class tab_bar;
class tab_head_item;

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
 */
class tab_bar : public container {
public:
    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;
    widget* hit_test_hover(float x, float y) const override;

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

    /// @brief 切换标签时内容区淡入。
    value_transition content_fade_{1.0f};

    float prev_indicator_pos_ = 0.0f;
    float prev_indicator_width_ = 0.0f;
    color_transition prev_indicator_alpha_{color::transparent()};

    void start_indicator_fade(float new_pos, float new_width);

    static constexpr float TAB_HEADER_H = 30.0f;
    static constexpr float TAB_FIXED_W = 120.0f;
};

} 
