/**
 * @file input_dialog.h
 * @brief 模态输入对话框。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/button.h>
#include <ui/text_field.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>
#include <functional>
#include <string>

namespace spiration {

/**
 * @brief 模态输入对话框。
 */
class input_dialog : public container {
public:
    input_dialog();

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void layout() override;
    void tick(float dt_ms) override;

    /**
     * @brief 显示对话框并聚焦输入框。
     * @param title 标题
     * @param placeholder 输入框占位提示
     * @param initial 初始文本
     * @param on_confirm 确定回调（参数为输入文本）
     * @param on_cancel 取消回调
     */
    void show(const std::string& title, const std::string& placeholder,
              const std::string& initial,
              std::function<void(const std::string&)> on_confirm,
              std::function<void()> on_cancel);
    void dismiss();
    bool visible() const { return visible_; }
    text_field* field() const { return field_; }

private:
    bool visible_ = false;
    std::string title_;
    std::function<void(const std::string&)> on_confirm_;
    std::function<void()> on_cancel_;
    value_transition fade_{0.0f};
    text_field* field_ = nullptr;
    button* ok_ = nullptr;
    button* cancel_ = nullptr;
    rectangle box_rect_{0, 0, 0, 0};
    rectangle field_rect_{0, 0, 0, 0};
    rectangle ok_rect_{0, 0, 0, 0};
    rectangle cancel_rect_{0, 0, 0, 0};

    void confirm();
    void cancel();
    void recalc_geometry();
};

}
