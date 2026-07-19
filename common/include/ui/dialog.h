/**
 * @file dialog.h
 * @brief 模态对话框基类。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/layout.h>
#include <ui/button.h>
#include <ui/theme.h>
#include <functional>

namespace spiration {

/**
 * @brief 模态对话框，覆盖在当前界面之上。
 */
class dialog : public container {
public:
    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;

    std::string title;
    std::function<void()> on_close;

    void show();
    void dismiss();
    bool visible() const { return visible_; }

    widget* content_area() const { return content_; }

protected:
    bool visible_ = false;
    widget* content_ = nullptr;
    float title_bar_h_ = 34.0f;
};

/**
 * @brief 简易消息对话框。
 */
class message_dialog : public dialog {
public:
    void set_message(const std::string& msg);
    void add_button(const std::string& label, std::function<void()> callback);
};

}
