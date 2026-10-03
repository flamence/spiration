/**
 * @file internal_ui.h
 * @brief `spiration.ui` 可选接口的内部共享声明（宿主私有）。
 * @author clk
 *
 * C API 的句柄（`spiration_widget*`、`spiration_tab*`）由包装对象充当：
 * 包装对象既是控件树中的一员，又参与存活登记，因此当宿主销毁控件（例如
 * 关闭标签页）之后，残留句柄会被安全地识别为失效，而不会解引用悬空指针。
 *
 * 包装类型在此声明，供 `widget.cpp` / `tab.cpp` / `ui.cpp` 三个翻译单元共享。
 *
 * @note 本头文件仅供 `spiration.core` 内部使用，不随 SDK 安装。
 */

#pragma once

#include <spiration/capi.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/ui/widget.h>

#include <memory>
#include <set>
#include <vector>

namespace spiration {

class root;

namespace capi {

/* -------------------------------------------------------------------------
 * 存活登记：句柄有效性判定只比较指针值，不解引用
 * ---------------------------------------------------------------------- */

/** @brief 存活控件句柄集合。 */
std::set<const void*>& live_widgets();

/** @brief 存活标签页句柄集合。 */
std::set<const void*>& live_tabs();

/** @brief 由某拓展创建的界面资源登记。 */
struct owned_tab {
    /** 创建者。 */
    const spiration_extension_desc* owner = nullptr;
    /** 标签页句柄。 */
    void* handle = nullptr;
};

/**
 * @brief 标签页归属登记。
 * @note 拓展卸载时据此关闭其标签页，避免宿主之后回调进入已卸载的动态库。
 */
std::vector<owned_tab>& owned_tabs();

/* -------------------------------------------------------------------------
 * 包装类型
 * ---------------------------------------------------------------------- */

/**
 * @brief 控件句柄：包装真正的控件，并在销毁时注销自身。
 */
class widget_handle : public widget {
public:
    explicit widget_handle(std::unique_ptr<widget> inner) {
        if (inner) {
            inner_ = inner.get();
            add_child(std::move(inner));
        }
        live_widgets().insert(this);
    }

    ~widget_handle() override { live_widgets().erase(this); }

    /** @brief 被包装的真实控件。 */
    widget* inner() const { return inner_; }

    void layout() override {
        if (inner_ != nullptr) {
            /* 真实控件填满包装体，其余交由自身布局逻辑处理。 */
            inner_->x = 0.0f;
            inner_->y = 0.0f;
            inner_->width = width;
            inner_->height = height;
        }
        widget::layout();
    }

    size layout_preferred_size() const override {
        return inner_ != nullptr ? inner_->layout_preferred_size() : size();
    }

private:
    widget* inner_ = nullptr;
};

/**
 * @brief 标签页句柄：内容为一个控件子树。
 */
class tab_handle : public tab {
public:
    tab_handle() { live_tabs().insert(this); }
    ~tab_handle() override { live_tabs().erase(this); }

    /** @brief 设置内容（接管所有权）。 */
    void set_content(std::unique_ptr<widget> content) {
        content_ = nullptr;
        if (!content) return;
        content_ = add_child(std::move(content));
    }

    /** @brief 当前内容。 */
    widget* content() const { return content_; }

    void paint(std::shared_ptr<renderer> renderer) override {
        container::paint(renderer);
    }

    void layout() override {
        if (content_ != nullptr) {
            content_->x = 0.0f;
            content_->y = 0.0f;
            content_->width = width;
            content_->height = height;
        }
        container::layout();
    }

private:
    widget* content_ = nullptr;
};

/* -------------------------------------------------------------------------
 * 句柄还原
 * ---------------------------------------------------------------------- */

/** @brief 校验控件句柄；失效返回 `nullptr`。 */
inline widget_handle* as_handle(spiration_widget* handle) {
    if (handle == nullptr) return nullptr;
    if (live_widgets().count(handle) == 0u) return nullptr;
    /* 不透明句柄是仅声明未定义的类型，故只能做指针层面的还原。 */
    return reinterpret_cast<widget_handle*>(handle);
}

/** @brief 校验只读控件句柄；失效返回 `nullptr`。 */
inline const widget_handle* as_handle(const spiration_widget* handle) {
    return as_handle(const_cast<spiration_widget*>(handle));
}

/** @brief 校验标签页句柄；失效返回 `nullptr`。 */
inline tab_handle* as_handle(spiration_tab* handle) {
    if (handle == nullptr) return nullptr;
    if (live_tabs().count(handle) == 0u) return nullptr;
    return reinterpret_cast<tab_handle*>(handle);
}

/* -------------------------------------------------------------------------
 * 公共操作
 * ---------------------------------------------------------------------- */

/** @brief 请求重绘窗口。 */
void repaint();

/** @brief 取标签栏，并回填根控件。 */
tab_bar* tab_strip(root*& out_root);

/**
 * @brief 触发一次控件事件回调。
 * @note 由 `widget.cpp` 注册的各控件回调调用。
 */
void emit_widget_event(spiration_widget_callback callback, void* user_data,
                       widget_handle* self, const char* event,
                       spiration_value data);

/* -------------------------------------------------------------------------
 * 函数表条目
 * ---------------------------------------------------------------------- */

/**
 * @name `spiration_ui_api` 的条目实现
 *
 * 声明集中于此，使 `widget.cpp` / `tab.cpp` 中的定义与 `ui.cpp` 中的组装
 * 互相校验；这些符号只在 `spiration.core` 内部使用。
 * @{
 */

/** @brief 定义于 `tab.cpp`。 */
spiration_status SPIRATION_CALL ui_add_menu_item(
    const spiration_extension_desc* self, spiration_str_view menu,
    spiration_str_view label, spiration_callback callback, void* user_data);

spiration_tab* SPIRATION_CALL ui_tab_open(const spiration_extension_desc* self,
                                          spiration_str_view title);
void SPIRATION_CALL ui_tab_close(spiration_tab* handle);
spiration_status SPIRATION_CALL ui_tab_activate(spiration_tab* handle);
spiration_status SPIRATION_CALL ui_tab_set_title(spiration_tab* handle,
                                                 spiration_str_view title);
spiration_status SPIRATION_CALL ui_tab_set_content(spiration_tab* handle,
                                                   spiration_widget* content);
spiration_bool SPIRATION_CALL ui_tab_is_alive(const spiration_tab* handle);

/** @brief 定义于 `widget.cpp`。 */
spiration_widget* SPIRATION_CALL ui_widget_create(spiration_widget_kind kind);
void SPIRATION_CALL ui_widget_destroy(spiration_widget* handle);
spiration_status SPIRATION_CALL ui_widget_append(spiration_widget* parent,
                                                 spiration_widget* child);
spiration_status SPIRATION_CALL ui_widget_set(spiration_widget* handle,
                                              spiration_str_view property,
                                              spiration_value value);
spiration_status SPIRATION_CALL ui_widget_get(const spiration_widget* handle,
                                              spiration_str_view property,
                                              spiration_value* out);
spiration_status SPIRATION_CALL ui_widget_listen(
    spiration_widget* handle, spiration_str_view event,
    spiration_widget_callback callback, void* user_data);

/** @} */

} // namespace capi
} // namespace spiration
