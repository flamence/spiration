/**
 * @file widget.h
 * @brief 控件基类定义。
 * @author clk
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <spiration/export.h>
#include <spiration/renderer/renderer.h>
#include <spiration/ui/style.h>
#include <spiration/window/event.h>
#include <vector>

namespace spiration {

class context_menu;
class root;

/**
 * @brief 所有 UI 控件的抽象基类。
 */
class SPIRATION_EXPORT widget {
private:
    widget* parent_ = nullptr;
    std::vector<std::unique_ptr<widget>> children_;

    float layout_x_ = 0.0f, layout_y_ = 0.0f;
    float layout_w_ = -1.0f, layout_h_ = -1.0f; // -1 哨兵：首次布局必执行
    bool layout_dirty_ = true;                  // 子树内容变更，需强制重排

    /// @brief 绘制裁剪区（本控件局部坐标），由父级在 paint 前下发。
    float paint_clip_l_ = 0.0f, paint_clip_t_ = 0.0f;
    float paint_clip_r_ = 0.0f, paint_clip_b_ = 0.0f;
    /// @brief 下发该裁剪区时的绘制轮次；与当前轮次不一致说明已过期，不得用于裁剪。
    uint64_t paint_clip_pass_ = 0;

    /// @brief 当前绘制轮次号（由 root 每帧递增）。
    static uint64_t paint_pass_;

public:
    
    float x = 0.0f, y = 0.0f;
    float width = 0.0f, height = 0.0f;

    bool enabled = true;
    bool focusable = false;
    bool visible = true;

private:
    /// @brief 子树中处于 hover 状态的控件数，用于指针移出时只向“有悬停残留”
    ///        的子树派发移动事件，避免每次鼠标移动遍历整棵树。
    int hovered_descendants_ = 0;

    void shift_hover_ancestors(int delta) {
        for (widget* p = parent_; p; p = p->parent_) p->hovered_descendants_ += delta;
    }

public:

    style widget_style;
    
    widget() {
        init();
    }

    // UI 控件不可拷贝（children_ 为 unique_ptr）。
    // 显式删除避免 dllexport 强制实例化隐式（已删除）拷贝导致编译错误。
    widget(const widget&) = delete;
    widget& operator=(const widget&) = delete;

    virtual ~widget() {
        dispose();
        notify_destroyed();
        children_.clear();
    }

    virtual void init() {  }

    virtual void dispose() {  }
    
    virtual void paint(std::shared_ptr<renderer> renderer) {
        for (auto& child : children_) {
            if (!child->visible) continue;
            // 绘制裁剪上下文：父级把可继承的裁剪区以子控件局部坐标下发，
            // 子控件据此 O(1) 判断自身是否完全不可见，避免逐层回溯祖先链。
            float cl, ct, cr, cb;
            if (!compute_child_paint_clip(*child, cl, ct, cr, cb)) continue;
            child->set_paint_clip(cl, ct, cr, cb);
            renderer->push_transform(child->x, child->y);
            child->paint(renderer);
            renderer->pop_transform();
        }
    }
    
    /**
     * @brief 开始一轮绘制。由 root 在每个绘制帧入口调用。
     *
     * 递增绘制轮次号用于校验控件上的裁剪上下文是否属于本轮：
     * 自定义 paint() 直接绘制子控件（不经过基类遍历）时不会下发新的裁剪区，
     * 此时子控件会检测到轮次不一致而退化为“不做裁剪”，保证不会误裁。
     */
    static void begin_paint_pass() { ++paint_pass_; }

    /**
     * @brief 父级在下发 paint 前设置本控件的可见裁剪区（本控件局部坐标）。
     *        该信息仅用于绘制期快速剔除，不参与命中测试。
     */
    void set_paint_clip(float left, float top, float right, float bottom) {
        paint_clip_l_ = left;
        paint_clip_t_ = top;
        paint_clip_r_ = right;
        paint_clip_b_ = bottom;
        paint_clip_pass_ = paint_pass_;
    }

    /**
     * @brief 清除裁剪上下文（浮层等直接由 root 绘制的控件使用）。
     */
    void clear_paint_clip() { paint_clip_pass_ = 0; }

    /**
     * @brief 本控件是否被裁剪区完全隐藏，绘制可整体跳过。
     */
    bool is_paint_clipped() const {
        return has_valid_paint_clip() &&
               (paint_clip_r_ <= paint_clip_l_ || paint_clip_b_ <= paint_clip_t_);
    }

protected:
    /**
     * @brief 计算子控件在本轮绘制中的可见裁剪区（子控件局部坐标）。
     *
     * 综合三部分信息，全部为 O(1)（旧实现 widget::visible_rect() 需要回溯祖先链，
     * 每个子控件 O(深度)，整帧 O(控件数 × 深度)）：
     *  1. 子控件自身矩形；
     *  2. 本控件矩形（与旧实现的祖先边界裁剪语义一致）；
     *  3. 本控件从父级继承的裁剪区（滚动容器需按滚动偏移反向平移）。
     *
     * @return false 表示子控件完全不可见，调用方可跳过绘制。
     */
    bool compute_child_paint_clip(const widget& child, float& l, float& t, float& r, float& b) const;

    /// @brief 本控件是否持有本轮的裁剪上下文。
    bool has_valid_paint_clip() const {
        return paint_clip_pass_ != 0 && paint_clip_pass_ == paint_pass_;
    }

public:
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
        on_layout_begin();
        for (auto& child : children_) {
            if (child->needs_layout()) {
                child->layout();
            }
        }
    }

    /**
     * @brief 本控件子树是否需要重新布局（几何变化或内容变更）。
     *        布局管理器/父容器据此跳过未变化的子树，避免全树重排。
     */
    bool needs_layout() const {
        return layout_dirty_ || bounds_changed_since_layout();
    }

    /**
     * @brief 标记本控件及全部祖先需要重新布局（内容变更时调用）。
     */
    void invalidate_layout() {
        for (widget* w = this; w; w = w->parent_) w->layout_dirty_ = true;
    }

    /**
     * @brief 几何相对上次布局是否发生变化。
     */
    bool bounds_changed_since_layout() const {
        return x != layout_x_ || y != layout_y_ ||
               width != layout_w_ || height != layout_h_;
    }

protected:
    /**
     * @brief 布局开始钩子：记录本次几何并清除脏标记。
     *        重写 layout() 的容器应在入口调用（或经由基类 widget::layout()）。
     */
    void on_layout_begin() {
        layout_dirty_ = false;
        layout_x_ = x;
        layout_y_ = y;
        layout_w_ = width;
        layout_h_ = height;
    }

public:    
    virtual void handle_event(const event_type& type, void* data) {
        if (!enabled) return;
        if (type == event_type::mouse) {
            auto* mouse_data = static_cast<mouse_event_data*>(data);
            point original = mouse_data->position;

            bool was_hover = hovered_;
            hovered_ = (original.x >= 0.0f && original.x <= width &&
                         original.y >= 0.0f && original.y <= height);
            if (hovered_ != was_hover) {
                shift_hover_ancestors(hovered_ ? 1 : -1);
                on_hover_change(hovered_);
                // 悬停态属于控件自身视觉状态，变化时必须自行请求重绘。
                // 窗口层因此不再对每条鼠标移动消息强制整窗重绘（见 Windows 消息循环帧调度），
                // 否则高频鼠标移动会持续触发全树重绘。
                if (request_repaint_) request_repaint_();
            }

            for (auto& child : children_) {
                if (!child->visible) continue;
                mouse_data->position.x = original.x - child->x;
                mouse_data->position.y = original.y - child->y;
                // 鼠标事件只派发给指针所在（或仍有悬停残留）的子树：
                // 旧实现无条件递归全部子控件，控件多时每次移动都是全树遍历。
                bool inside = mouse_data->position.x >= 0.0f &&
                              mouse_data->position.x <= child->width &&
                              mouse_data->position.y >= 0.0f &&
                              mouse_data->position.y <= child->height;
                if (!inside && !child->is_hovered() && !child->has_hovered_descendant()) continue;
                child->handle_event(type, data);
                if (mouse_data->consumed) break;
            }
            mouse_data->position = original;
        } else if (type == event_type::keyboard) {
            for (auto& child : children_) {
                if (!child->visible) continue;
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

    /// @brief 子树中是否仍有控件处于悬停态（用于鼠标移出时的收尾派发）。
    bool has_hovered_descendant() const { return hovered_descendants_ > 0; }

    /**
     * @brief 布局器写完子控件最终几何后调用：同步已记录几何并清除脏标记。
     *        避免“测量时 layout 一次、定位后又 layout 一次”的重复重排。
     */
    void finish_layout_geometry() { on_layout_begin(); }

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
            if (!child->visible) continue;
            float cx = x - child->x;
            float cy = y - child->y;
            // 先做矩形包含判断：旧实现无条件递归整棵子树。
            if (cx < 0.0f || cx > child->width || cy < 0.0f || cy > child->height) continue;
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
        // 延迟添加的控件必须继承宿主已注册的回调，否则重绘/窗口动作/右键菜单失效。
        if (request_repaint_) rawPtr->set_repaint_callback(request_repaint_);
        if (window_action_) rawPtr->set_window_action_callback(window_action_);
        if (context_menu_cb_) rawPtr->set_context_menu_callback(context_menu_cb_);
        children_.push_back(std::move(child));
        invalidate_layout(); // 子项变化，父链需要重排
        return rawPtr;
    }
    
    std::unique_ptr<widget> remove_child(widget* child) {
        auto it = std::find_if(children_.begin(), children_.end(),
            [child](const std::unique_ptr<widget>& ptr) {
                return ptr.get() == child;
            });
        
        if (it != children_.end()) {
            std::unique_ptr<widget> removed = std::move(*it);
            removed->notify_destroyed_recursive();
            removed->parent_ = nullptr;
            children_.erase(it);
            invalidate_layout(); // 子项变化，父链需要重排
            return removed;
        }
        return nullptr;
    }

    /**
     * @brief 一次性移除全部子控件（O(n)），避免逐个 remove_child 的 O(n²) 与重复标脏。
     */
    void clear_children() {
        if (children_.empty()) return;
        for (auto& child : children_) {
            child->notify_destroyed_recursive();
            child->parent_ = nullptr;
        }
        children_.clear();
        invalidate_layout();
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
    virtual void set_repaint_callback(std::function<void()> cb) {
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
     * @brief 计算本控件在祖先滚动/裁剪下实际可见的区域。
     *        用于虚拟视图：绘制/tick 前判断子项是否在可视区域内。
     * @return false 表示完全不可见。
     */
    bool visible_rect(float& vx, float& vy, float& vw, float& vh) const {
        if (!parent_) {
            vx = 0.0f; vy = 0.0f; vw = width; vh = height;
            return vw > 0.0f && vh > 0.0f;
        }
        float px, py, pw, ph;
        if (!parent_->visible_rect(px, py, pw, ph)) return false;
        const float sx = parent_->scroll_offset_x_for_children();
        const float sy = parent_->scroll_offset_for_children();
        const float x0 = std::max(px + sx - x, 0.0f);
        const float y0 = std::max(py + sy - y, 0.0f);
        const float x1 = std::min(px + sx - x + pw, width);
        const float y1 = std::min(py + sy - y + ph, height);
        if (x1 <= x0 || y1 <= y0) return false;
        vx = x0; vy = y0; vw = x1 - x0; vh = y1 - y0;
        return true;
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
     * @brief 通知 root 清理可能指向本控件的捕获/选区指针。
     *        防止 root 保存的裸指针在控件销毁后悬空导致 UAF 崩溃。
     */
    void notify_destroyed();

    /**
     * @brief 递归通知本控件及其全部子孙。
     */
    void notify_destroyed_recursive();

    /**
     * @brief 控件悬停时应显示的光标。
     */
    virtual cursor_style effective_cursor(float lx, float ly) const {
        (void)lx; (void)ly;
        return widget_style.cursor;
    }

    /**
     * @brief 沿父链重新布局。
     *        供动画驱动的控件在高度变化时调用，让父布局及时跟随。
     * @param max_depth 最多重排的祖先层数（含自身）；默认不限。
     */
    void relayout_chain(unsigned max_depth = 0xFFFFFFFFu);

    /**
     * @brief 沿父链查找根节点，找不到返回 nullptr。
     *        浮层控件用它注册为顶层事件接收者。
     */
    root* find_root();

    void set_parent(widget* p) { parent_ = p; }

    virtual void set_mouse_capture(widget* w) {
        if (parent_) parent_->set_mouse_capture(w);
    }

    virtual void on_focus() {}
    virtual void on_blur() {}

    virtual bool is_focusable() const { return focusable && enabled; }
    /**
     * @brief 设置可见性并联动焦点与重绘。
     * @param v 是否可见
     */
    void set_visible(bool v);

    /**
     * @brief 设置启用状态并联动焦点与重绘。
     * @param v 是否启用
     */
    void set_enabled(bool v);
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
    virtual void set_window_action_callback(std::function<void(int)> cb) {
        window_action_ = cb;
        for (auto& child : children_) {
            child->set_window_action_callback(cb);
        }
    }

    
    const std::function<void(int)>& get_window_action_callback() const { return window_action_; }
};

}
