/**
 * @file tab_bar.h
 * @brief 标签栏与标签控件。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

class tab_bar;

/**
 * @brief 单个标签页。
 *
 * 每个 tab 有自己的标题和内容区域，由 tab_bar 管理生命周期。
 */
class tab : public container {
    friend class tab_bar;
public:
    void set_title(const std::string& t) { title_ = t; }
    const std::string& title() const { return title_; }

    bool is_active() const { return active_; }

    
    virtual void on_activate() {}
    virtual void on_deactivate() {}

    void paint(std::shared_ptr<renderer> renderer) override = 0;

protected:
    std::string title_;
    bool active_ = false;
};

/**
 * @brief 标签栏，管理多个 tab，显示标签头并切换内容区域。
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
    int hovered_close_idx_ = -1;  

    static constexpr float TAB_HEADER_H = 30.0f;
    static constexpr float TAB_MIN_W = 100.0f;
    static constexpr float CLOSE_BTN_W = 20.0f;

    int hit_test_tab_header(float mx, float my) const;
    int hit_test_close_btn(float mx, float my) const;
};

} 
