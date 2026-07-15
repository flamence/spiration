/**
 * @file widget.h
 * @brief Widget 基类定义，所有 UI 控件的基类。
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
            child->paint(renderer);
        }
    }
    
    /**
     * @brief 返回此 widget 的首选（理想）尺寸。
     *        布局管理器会参考此值来决定 widget 的分配空间。
     */
    virtual size layout_preferred_size() const {
        return {width, height};
    }

    /**
     * @brief 每帧更新钩子，用于驱动动画等时间相关逻辑。
     * @param dt_ms 距离上一帧的毫秒数
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
        if (type == event_type::mouse) {
            auto* mouse_data = static_cast<mouse_event_data*>(data);
            point original = mouse_data->position;
            for (auto& child : children_) {
                mouse_data->position.x = original.x - child->x;
                mouse_data->position.y = original.y - child->y;
                child->handle_event(type, data);
            }
            mouse_data->position = original;
        } else {
            for (auto& child : children_) {
                child->handle_event(type, data);
            }
        }
    }

    /**
     * @brief 检测当前 widget 自身是否消耗了指定坐标的鼠标点击。
     * @return true 表示该 widget 在此位置是可交互的，不应穿透触发窗口拖拽
     */
    virtual bool hit_test(float x, float y) const {
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

    void set_parent(widget* p) { parent_ = p; }

    virtual void set_mouse_capture(widget* w) {
        if (parent_) parent_->set_mouse_capture(w);
    }

protected:
    
    std::function<void()> request_repaint_ = nullptr;

    
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