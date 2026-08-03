/**
 * @file widget.h
 * @brief 控件基类定义。
 * @author clk
 */

#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <renderer/renderer.h>
#include <ui/style.h>
#include <utils/console.h>
#include <window/event.h>
#include <vector>

namespace spiration {

class context_menu;

/**
 * @brief 所有 UI 控件的抽象基类。
 */
class widget {
private:
    widget* parent_ = nullptr;
    std::vector<std::unique_ptr<widget>> children_;

public:
    
    float x = 0.0f, y = 0.0f;
    float width = 0.0f, height = 0.0f;

    bool enabled = true;
    bool focusable = false;

    style widget_style;
    
    widget() {
        init();
    }

    virtual ~widget() {
        dispose();
        for (auto& child : children_) {
            child->parent_ = nullptr;
        }
    }

    virtual void init() {  }

    virtual void dispose() {  }
    
    virtual void paint(std::shared_ptr<renderer> renderer) {
        for (auto& child : children_) {
            renderer->push_transform(child->x, child->y);
            child->paint(renderer);
            renderer->pop_transform();
        }
    }
    
    /**
     * @brief 返回此 widget 的首选尺寸。
     *        布局管理器会参考此值来决定 widget 的分配空间。
     */
    virtual size layout_preferred_size() const {
        return {0.0f, 0.0f};
    }

    /**
     * @brief 每帧更新钩子，用于驱动动画等时间相关逻辑。
     * @param dt_ms 距离上一帧的毫秒数。
     */
    virtual void tick(float dt_ms) {
        for (auto& child : children_) {
            child->tick(dt_ms);
        }
    }

    virtual void layout() {
        for (auto& child : children_) {
            child->layout();
        }
    }
    
    virtual void handle_event(const event_type& type, void* data) {
        if (!enabled) return;
        if (type == event_type::mouse) {
            auto* mouse_data = static_cast<mouse_event_data*>(data);
            point original = mouse_data->position;

            bool was_hover = hovered_;
            hovered_ = (original.x >= 0.0f && original.x <= width &&
                         original.y >= 0.0f && original.y <= height);
            if (hovered_ != was_hover) on_hover_change(hovered_);

            for (auto& child : children_) {
                mouse_data->position.x = original.x - child->x;
                mouse_data->position.y = original.y - child->y;
                child->handle_event(type, data);
                if (mouse_data->consumed) break;
            }
            mouse_data->position = original;
        } else if (type == event_type::keyboard) {
            for (auto& child : children_) {
                child->handle_event(type, data);
                if (static_cast<key_event_data*>(data)->consumed) break;
            }
        } else {
            for (auto& child : children_) {
                child->handle_event(type, data);
            }
        }
    }

    virtual void on_hover_change(bool hovered) { (void)hovered; }

    bool is_hovered() const { return hovered_; }

    /**
     * @brief 检测当前 widget 自身是否消耗了指定坐标的鼠标点击。
     * @return true 表示该 widget 在此位置是可交互的，不应穿透触发窗口拖拽
     */
    virtual bool hit_test(float x, float y) const {
        (void)x; (void)y;
        return false;
    }

    /**
     * @brief 递归检测 widget 树中是否有任意子 widget 在 (x, y) 处是可交互的。
     *        坐标相对于当前 widget 的父容器。
     */
    bool hit_test_children(float x, float y) const {
        for (const auto& child : children_) {
            float cx = x - child->x;
            float cy = y - child->y;
            if (child->hit_test(cx, cy)) return true;
            if (child->hit_test_children(cx, cy)) return true;
        }
        return false;
    }

    /**
     * @brief 命中测试：返回包含 (x, y) 的最深层 enabled 控件。
     *        坐标为本控件的局部坐标；递归时自动补偿各层滚动偏移。
     */
    virtual widget* hit_test_hover(float x, float y) const {
        if (!enabled) return nullptr;
        if (x < 0.0f || x > width || y < 0.0f || y > height) return nullptr;
        const float sox = scroll_offset_x_for_children();
        const float soy = scroll_offset_for_children();
        for (const auto& child : children_) {
            const float cx = x - child->x + sox;
            const float cy = y - child->y + soy;
            if (widget* h = child->hit_test_hover(cx, cy)) return h;
        }
        return const_cast<widget*>(this);
    }

    widget* add_child(std::unique_ptr<widget> child) {
        child->parent_ = this;
        widget* rawPtr = child.get();
        children_.push_back(std::move(child));
        return rawPtr;
    }
    
    std::unique_ptr<widget> remove_child(widget* child) {
        auto it = std::find_if(children_.begin(), children_.end(),
            [child](const std::unique_ptr<widget>& ptr) {
                return ptr.get() == child;
            });
        
        if (it != children_.end()) {
            std::unique_ptr<widget> removed = std::move(*it);
            removed->parent_ = nullptr;
            children_.erase(it);
            return removed;
        }
        return nullptr;
    }

    widget* parent() const {
        return parent_;
    }
    
    const std::vector<std::unique_ptr<widget>>& children() const {
        return children_;
    }

    /**
     * @brief 为当前 widget 及其所有子 widget 注册重绘回调。
     * @param cb 无参回调，调用时触发窗口重绘
     */
    void set_repaint_callback(std::function<void()> cb) {
        request_repaint_ = cb;
        for (auto& child : children_) {
            child->set_repaint_callback(cb);
        }
    }

    /**
     * @brief 设置右键上下文菜单请求回调（沿子树递归传播）。
     * 实现在 widget.cpp（需要 context_menu 完整类型）。
     */
    void set_context_menu_callback(std::function<void(float x, float y, std::unique_ptr<context_menu>)> cb);

    /**
     * @brief 请求在窗口屏幕坐标 (x, y) 处显示右键菜单。
     * 沿父链向上查找最近设置了回调的节点（root 通常持有）。
     * 实现在 widget.cpp。
     */
    void request_context_menu(float x, float y, std::unique_ptr<context_menu> menu);

    /**
     * @brief 计算控件内局部坐标对应的窗口屏幕坐标。
     */
    point to_screen(float x, float y) const {
        point p{x, y};
        const widget* w = this;
        while (w) {
            p.x += w->x - w->scroll_offset_x_for_children();
            p.y += w->y - w->scroll_offset_for_children();
            w = w->parent_;
        }
        return p;
    }

    /**
     * @brief 子内容的垂直滚动偏移。
     */
    virtual float scroll_offset_for_children() const { return 0.0f; }

    /**
     * @brief 子内容的水平滚动偏移。
     */
    virtual float scroll_offset_x_for_children() const { return 0.0f; }

    /**
     * @brief 清除控件的文本选区。
     */
    virtual void clear_text_selection() {}

    /**
     * @brief 控件悬停时应显示的光标。
     */
    virtual cursor_type effective_cursor() const { return widget_style.cursor; }

    void set_parent(widget* p) { parent_ = p; }

    virtual void set_mouse_capture(widget* w) {
        if (parent_) parent_->set_mouse_capture(w);
    }

    virtual void on_focus() {}
    virtual void on_blur() {}

    virtual bool is_focusable() const { return focusable && enabled; }

    void set_focused(bool f) {
        if (f == focused_) return;
        focused_ = f;
        if (f) on_focus(); else on_blur();
    }
    bool is_focused() const { return focused_; }

protected:
    
    std::function<void()> request_repaint_ = nullptr;
    std::function<void(float, float, std::unique_ptr<context_menu>)> context_menu_cb_ = nullptr;

    bool focused_ = false;
    bool hovered_ = false;

    
    std::function<void(int action)> window_action_ = nullptr;

public:
    
    enum window_action {
        action_minimize = 0,
        action_maximize,
        action_restore,
        action_close,
    };

    /**
     * @brief 注册窗口动作回调，递归设置整棵子树。
     */
    void set_window_action_callback(std::function<void(int)> cb) {
        window_action_ = cb;
        for (auto& child : children_) {
            child->set_window_action_callback(cb);
        }
    }

    
    const std::function<void(int)>& get_window_action_callback() const { return window_action_; }
};

}