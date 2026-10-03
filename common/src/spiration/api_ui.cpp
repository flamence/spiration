/**
 * @file api_ui.cpp
 * @brief 可选接口 `spiration.ui` 的宿主侧实现。
 * @author clk
 *
 * C API 的句柄（`spiration_widget*`、`spiration_tab*`）在这里由包装对象充当：
 * 包装对象既是控件树中的一员，又参与存活登记，因此当宿主销毁控件（例如
 * 关闭标签页）之后，残留句柄会被安全地识别为失效，而不会解引用悬空指针。
 */

#include <spiration/capi.h>

#include "capi_util.h"

#include <spiration/api.h>
#include <spiration/application.h>
#include <spiration/ui/button.h>
#include <spiration/ui/checkbox.h>
#include <spiration/ui/container.h>
#include <spiration/ui/label.h>
#include <spiration/ui/layout.h>
#include <spiration/ui/progress_bar.h>
#include <spiration/ui/root.h>
#include <spiration/ui/separator.h>
#include <spiration/ui/slider.h>
#include <spiration/ui/spacer.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/ui/text_area.h>
#include <spiration/ui/text_field.h>
#include <spiration/window/window.h>

#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace spiration {
namespace capi {

namespace {

using detail::text_of;
using detail::view_of;

// ---------------------------------------------------------------------------
// 存活登记：句柄有效性判定只比较指针值，不解引用
// ---------------------------------------------------------------------------

/** @brief 存活控件句柄集合。 */
std::set<const void*>& live_widgets() {
    static std::set<const void*> handles;
    return handles;
}

/** @brief 存活标签页句柄集合。 */
std::set<const void*>& live_tabs() {
    static std::set<const void*> handles;
    return handles;
}

/**
 * @brief 由某拓展创建的界面资源登记。
 * @note 拓展卸载时据此关闭其标签页并移除其菜单项，避免宿主之后回调
 *       进入已卸载的动态库。
 */
struct owned_tab {
    /** 创建者。 */
    const spiration_extension_desc* owner = nullptr;
    /** 标签页句柄。 */
    void* handle = nullptr;
};

/** @brief 标签页归属登记。 */
std::vector<owned_tab>& owned_tabs() {
    static std::vector<owned_tab> entries;
    return entries;
}

/** @brief 请求重绘窗口。 */
void repaint() {
    application* app = application::instance();
    if (app != nullptr && app->window() != nullptr) {
        app->window()->request_repaint();
    }
}

// ---------------------------------------------------------------------------
// 控件包装
// ---------------------------------------------------------------------------

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

/** @brief 校验控件句柄。 */
widget_handle* as_handle(spiration_widget* handle) {
    if (handle == nullptr) return nullptr;
    if (live_widgets().count(handle) == 0u) return nullptr;
    /* 不透明句柄是仅声明未定义的类型，故只能做指针层面的还原。 */
    return reinterpret_cast<widget_handle*>(handle);
}

/** @brief 校验只读控件句柄。 */
const widget_handle* as_handle(const spiration_widget* handle) {
    return as_handle(const_cast<spiration_widget*>(handle));
}

// ---------------------------------------------------------------------------
// 标签页
// ---------------------------------------------------------------------------

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

/** @brief 校验标签页句柄。 */
tab_handle* as_handle(spiration_tab* handle) {
    if (handle == nullptr) return nullptr;
    if (live_tabs().count(handle) == 0u) return nullptr;
    return reinterpret_cast<tab_handle*>(handle);
}

/** @brief 取标签栏。 */
tab_bar* tab_strip(root*& out_root) {
    out_root = dynamic_cast<root*>(application::instance()->widget());
    if (out_root == nullptr) return nullptr;
    return out_root->get_tab_bar();
}

// ---------------------------------------------------------------------------
// 属性
// ---------------------------------------------------------------------------

/** @brief 文本属性写入。 */
spiration_status set_text_property(widget* inner, const std::string& text) {
    if (inner == nullptr) return SPIRATION_ERR_NOT_FOUND;

    /* 输入控件先于标签处理：其内部缓存需要复位。 */
    if (text_area* area = dynamic_cast<text_area*>(inner)) {
        area->set_text(text);
        area->reset_view();
        return SPIRATION_OK;
    }
    if (label* text_label = dynamic_cast<label*>(inner)) {
        text_label->set_text(text);
        return SPIRATION_OK;
    }
    if (button* push = dynamic_cast<button*>(inner)) {
        push->text = text;
        push->invalidate_layout();
        return SPIRATION_OK;
    }
    if (checkbox* box = dynamic_cast<checkbox*>(inner)) {
        box->text = text;
        box->invalidate_layout();
        return SPIRATION_OK;
    }
    return SPIRATION_ERR_UNSUPPORTED;
}

/** @brief 文本属性读取。 */
spiration_status get_text_property(const widget* inner, spiration_value* out) {
    if (inner == nullptr || out == nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT;
    }
    if (const text_area* area = dynamic_cast<const text_area*>(inner)) {
        *out = spiration_value_string(spiration_str_view_of(
            area->text.data(), area->text.size()));
        return SPIRATION_OK;
    }
    if (const label* text_label = dynamic_cast<const label*>(inner)) {
        *out = spiration_value_string(spiration_str_view_of(
            text_label->text.data(), text_label->text.size()));
        return SPIRATION_OK;
    }
    if (const button* push = dynamic_cast<const button*>(inner)) {
        *out = spiration_value_string(
            spiration_str_view_of(push->text.data(), push->text.size()));
        return SPIRATION_OK;
    }
    if (const checkbox* box = dynamic_cast<const checkbox*>(inner)) {
        *out = spiration_value_string(
            spiration_str_view_of(box->text.data(), box->text.size()));
        return SPIRATION_OK;
    }
    return SPIRATION_ERR_UNSUPPORTED;
}

/** @brief 写入布局管理器。 */
spiration_status set_layout_property(container* box, const std::string& layout) {
    if (box == nullptr) return SPIRATION_ERR_UNSUPPORTED;
    if (layout == "row") {
        box->set_layout_manager(std::unique_ptr<layout_manager>(new horizontal_layout(8.0f)));
        return SPIRATION_OK;
    }
    if (layout == "column") {
        box->set_layout_manager(std::unique_ptr<layout_manager>(new vertical_layout(8.0f)));
        return SPIRATION_OK;
    }
    if (layout == "none") {
        box->set_layout_manager(nullptr);
        return SPIRATION_OK;
    }
    return SPIRATION_ERR_UNSUPPORTED;
}

spiration_status SPIRATION_CALL ui_widget_set(spiration_widget* handle,
                                              spiration_str_view property,
                                              spiration_value value) {
    widget_handle* box = as_handle(handle);
    if (box == nullptr) return SPIRATION_ERR_NOT_FOUND;

    const std::string key = text_of(property);
    widget* inner = box->inner();

    if (key == "x" || key == "y" || key == "width" || key == "height") {
        if (value.type != SPIRATION_VALUE_FLOAT &&
            value.type != SPIRATION_VALUE_INT) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        const float number = value.type == SPIRATION_VALUE_FLOAT
                                 ? static_cast<float>(value.data.number)
                                 : static_cast<float>(value.data.integer);
        if (key == "x") box->x = number;
        else if (key == "y") box->y = number;
        else if (key == "width") box->width = number;
        else box->height = number;
        box->invalidate_layout();
        repaint();
        return SPIRATION_OK;
    }

    if (key == "visible" || key == "enabled") {
        if (value.type != SPIRATION_VALUE_BOOL) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        const bool flag = value.data.boolean == SPIRATION_TRUE;
        if (key == "visible") box->set_visible(flag);
        else box->set_enabled(flag);
        repaint();
        return SPIRATION_OK;
    }

    if (key == "text") {
        if (value.type != SPIRATION_VALUE_STRING) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        const spiration_status status =
            set_text_property(inner, text_of(value.data.text));
        if (status == SPIRATION_OK) {
            box->invalidate_layout();
            repaint();
        }
        return status;
    }

    if (key == "placeholder") {
        if (value.type != SPIRATION_VALUE_STRING) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        if (text_area* area = dynamic_cast<text_area*>(inner)) {
            area->placeholder = text_of(value.data.text);
            repaint();
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    if (key == "selectable") {
        if (value.type != SPIRATION_VALUE_BOOL) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        if (label* text_label = dynamic_cast<label*>(inner)) {
            text_label->selectable = value.data.boolean == SPIRATION_TRUE;
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    if (key == "checked") {
        if (value.type != SPIRATION_VALUE_BOOL) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        if (checkbox* box_widget = dynamic_cast<checkbox*>(inner)) {
            box_widget->checked = value.data.boolean == SPIRATION_TRUE;
            repaint();
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    if (key == "value") {
        if (value.type != SPIRATION_VALUE_FLOAT &&
            value.type != SPIRATION_VALUE_INT) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        const float number = value.type == SPIRATION_VALUE_FLOAT
                                 ? static_cast<float>(value.data.number)
                                 : static_cast<float>(value.data.integer);
        if (slider* track = dynamic_cast<slider*>(inner)) {
            track->value = number;
            repaint();
            return SPIRATION_OK;
        }
        if (progress_bar* bar = dynamic_cast<progress_bar*>(inner)) {
            bar->progress = number;
            repaint();
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    if (key == "min" || key == "max") {
        if (value.type != SPIRATION_VALUE_FLOAT &&
            value.type != SPIRATION_VALUE_INT) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        const float number = value.type == SPIRATION_VALUE_FLOAT
                                 ? static_cast<float>(value.data.number)
                                 : static_cast<float>(value.data.integer);
        if (slider* track = dynamic_cast<slider*>(inner)) {
            if (key == "min") track->min_value = number;
            else track->max_value = number;
            repaint();
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    if (key == "layout") {
        if (value.type != SPIRATION_VALUE_STRING) {
            return SPIRATION_ERR_INVALID_ARGUMENT;
        }
        container* box_widget =
            dynamic_cast<container*>(inner != nullptr ? inner : box);
        if (box_widget == nullptr) return SPIRATION_ERR_UNSUPPORTED;
        const spiration_status status =
            set_layout_property(box_widget, text_of(value.data.text));
        if (status == SPIRATION_OK) {
            box->invalidate_layout();
            repaint();
        }
        return status;
    }

    return SPIRATION_ERR_UNSUPPORTED;
}

spiration_status SPIRATION_CALL ui_widget_get(const spiration_widget* handle,
                                              spiration_str_view property,
                                              spiration_value* out) {
    const widget_handle* box = as_handle(handle);
    if (box == nullptr || out == nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT;
    }

    const std::string key = text_of(property);
    const widget* inner = box->inner();

    if (key == "x") { *out = spiration_value_float(box->x); return SPIRATION_OK; }
    if (key == "y") { *out = spiration_value_float(box->y); return SPIRATION_OK; }
    if (key == "width") { *out = spiration_value_float(box->width); return SPIRATION_OK; }
    if (key == "height") { *out = spiration_value_float(box->height); return SPIRATION_OK; }
    if (key == "visible") { *out = spiration_value_bool(box->visible ? SPIRATION_TRUE : SPIRATION_FALSE); return SPIRATION_OK; }
    if (key == "enabled") { *out = spiration_value_bool(box->enabled ? SPIRATION_TRUE : SPIRATION_FALSE); return SPIRATION_OK; }
    if (key == "text") { return get_text_property(inner, out); }

    if (key == "checked") {
        const checkbox* box_widget = dynamic_cast<const checkbox*>(inner);
        if (box_widget == nullptr) return SPIRATION_ERR_UNSUPPORTED;
        *out = spiration_value_bool(box_widget->checked ? SPIRATION_TRUE
                                                        : SPIRATION_FALSE);
        return SPIRATION_OK;
    }

    if (key == "value") {
        if (const slider* track = dynamic_cast<const slider*>(inner)) {
            *out = spiration_value_float(track->value);
            return SPIRATION_OK;
        }
        if (const progress_bar* bar = dynamic_cast<const progress_bar*>(inner)) {
            *out = spiration_value_float(bar->progress);
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    return SPIRATION_ERR_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------

/** @brief 触发一次控件事件回调。 */
void emit_widget_event(spiration_widget_callback callback, void* user_data,
                       widget_handle* self, const char* event,
                       spiration_value data) {
    if (callback == nullptr) return;
    callback(user_data, reinterpret_cast<spiration_widget*>(self),
             spiration_str_view_of(event, std::strlen(event)), data);
}

spiration_status SPIRATION_CALL ui_widget_listen(
    spiration_widget* handle, spiration_str_view event,
    spiration_widget_callback callback, void* user_data) {
    widget_handle* box = as_handle(handle);
    if (box == nullptr || callback == nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT;
    }

    widget* inner = box->inner();
    const std::string key = text_of(event);

    if (key == "click") {
        button* push = dynamic_cast<button*>(inner);
        if (push == nullptr) return SPIRATION_ERR_UNSUPPORTED;
        push->on_click = [callback, user_data, box]() {
            emit_widget_event(callback, user_data, box, "click",
                              spiration_value_none());
        };
        return SPIRATION_OK;
    }

    if (key == "changed") {
        if (checkbox* box_widget = dynamic_cast<checkbox*>(inner)) {
            box_widget->on_changed = [callback, user_data, box](bool checked) {
                emit_widget_event(callback, user_data, box, "changed",
                                  spiration_value_bool(checked ? SPIRATION_TRUE
                                                               : SPIRATION_FALSE));
            };
            return SPIRATION_OK;
        }
        if (slider* track = dynamic_cast<slider*>(inner)) {
            track->on_changed = [callback, user_data, box](float value) {
                emit_widget_event(callback, user_data, box, "changed",
                                  spiration_value_float(value));
            };
            return SPIRATION_OK;
        }
        if (text_area* area = dynamic_cast<text_area*>(inner)) {
            area->on_changed = [callback, user_data, box](
                                   const std::string& text) {
                emit_widget_event(callback, user_data, box, "changed",
                                  spiration_value_string(
                                      spiration_str_view_of(text.data(),
                                                            text.size())));
            };
            return SPIRATION_OK;
        }
        return SPIRATION_ERR_UNSUPPORTED;
    }

    if (key == "submit") {
        text_area* area = dynamic_cast<text_area*>(inner);
        if (area == nullptr) return SPIRATION_ERR_UNSUPPORTED;
        area->on_submit = [callback, user_data, box](const std::string& text) {
            emit_widget_event(callback, user_data, box, "submit",
                              spiration_value_string(spiration_str_view_of(
                                  text.data(), text.size())));
        };
        return SPIRATION_OK;
    }

    return SPIRATION_ERR_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// 工厂
// ---------------------------------------------------------------------------

/** @brief 按种类创建真实控件。 */
std::unique_ptr<widget> create_inner(spiration_widget_kind kind) {
    switch (kind) {
        case SPIRATION_WIDGET_CONTAINER:
            return std::unique_ptr<widget>(new container());
        case SPIRATION_WIDGET_LABEL:
            return std::unique_ptr<widget>(new label());
        case SPIRATION_WIDGET_BUTTON:
            return std::unique_ptr<widget>(new button());
        case SPIRATION_WIDGET_CHECKBOX:
            return std::unique_ptr<widget>(new checkbox());
        case SPIRATION_WIDGET_TEXT_FIELD:
            return std::unique_ptr<widget>(new text_field());
        case SPIRATION_WIDGET_TEXT_AREA:
            return std::unique_ptr<widget>(new text_area());
        case SPIRATION_WIDGET_SLIDER:
            return std::unique_ptr<widget>(new slider());
        case SPIRATION_WIDGET_PROGRESS_BAR:
            return std::unique_ptr<widget>(new progress_bar());
        case SPIRATION_WIDGET_SEPARATOR:
            return std::unique_ptr<widget>(new separator());
        case SPIRATION_WIDGET_SPACER:
            return std::unique_ptr<widget>(new spacer());
        default:
            /* 尚未映射的控件种类（下拉框、列表、折叠面板…）返回空，
               调用方据此得到 NULL 句柄。 */
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
// 界面函数实现
// ---------------------------------------------------------------------------

void SPIRATION_CALL ui_request_repaint(const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    repaint();
}

spiration_status SPIRATION_CALL ui_add_menu_item(
    const spiration_extension_desc* self, spiration_str_view menu,
    spiration_str_view label, spiration_callback callback, void* user_data) {
    if (callback == nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT;
    }

    root* root_widget = dynamic_cast<root*>(application::instance()->widget());
    if (root_widget == nullptr) {
        return SPIRATION_ERR_NOT_READY;
    }

    const std::string owner = text_of(self != nullptr ? self->id : spiration_str_view());
    const bool ok = root_widget->add_menu_item(
        text_of(menu), text_of(label),
        [callback, user_data]() { callback(user_data); }, owner);
    return ok ? SPIRATION_OK : SPIRATION_ERR_NOT_FOUND;
}

spiration_tab* SPIRATION_CALL ui_tab_open(
    const spiration_extension_desc* self, spiration_str_view title) {
    root* root_widget = dynamic_cast<root*>(application::instance()->widget());
    if (root_widget == nullptr) return NULL;

    std::unique_ptr<tab_handle> created(new tab_handle());
    created->set_title(text_of(title));

    tab_handle* raw = created.get();
    root_widget->open_tab(std::move(created));

    /* 登记归属，供拓展卸载时关闭。 */
    owned_tab entry;
    entry.owner = self;
    entry.handle = raw;
    owned_tabs().push_back(entry);

    return reinterpret_cast<spiration_tab*>(raw);
}

void SPIRATION_CALL ui_tab_close(spiration_tab* handle) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return;

    root* root_widget = nullptr;
    tab_bar* strip = tab_strip(root_widget);
    if (strip == nullptr) return;

    for (int i = 0; i < strip->tab_count(); ++i) {
        if (strip->get_tab(i) == target) {
            strip->close_tab(i);
            return;
        }
    }
}

spiration_status SPIRATION_CALL ui_tab_activate(spiration_tab* handle) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return SPIRATION_ERR_NOT_FOUND;

    root* root_widget = nullptr;
    tab_bar* strip = tab_strip(root_widget);
    if (strip == nullptr) return SPIRATION_ERR_NOT_READY;

    for (int i = 0; i < strip->tab_count(); ++i) {
        if (strip->get_tab(i) == target) {
            strip->activate_tab(i);
            return SPIRATION_OK;
        }
    }
    return SPIRATION_ERR_NOT_FOUND;
}

spiration_status SPIRATION_CALL ui_tab_set_title(spiration_tab* handle,
                                                 spiration_str_view title) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return SPIRATION_ERR_NOT_FOUND;
    target->set_title(text_of(title));
    repaint();
    return SPIRATION_OK;
}

spiration_status SPIRATION_CALL ui_tab_set_content(spiration_tab* handle,
                                                   spiration_widget* content) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return SPIRATION_ERR_NOT_FOUND;

    if (content == nullptr) {
        target->set_content(nullptr);
        repaint();
        return SPIRATION_OK;
    }

    widget_handle* box = as_handle(content);
    if (box == nullptr) return SPIRATION_ERR_NOT_FOUND;
    if (box->parent() != nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT; /* 已挂在某棵树上 */
    }

    target->set_content(std::unique_ptr<widget>(box));
    repaint();
    return SPIRATION_OK;
}

spiration_bool SPIRATION_CALL ui_tab_is_alive(const spiration_tab* handle) {
    if (handle == nullptr) return SPIRATION_FALSE;
    return live_tabs().count(handle) != 0u ? SPIRATION_TRUE : SPIRATION_FALSE;
}

spiration_widget* SPIRATION_CALL ui_widget_create(spiration_widget_kind kind) {
    std::unique_ptr<widget> inner = create_inner(kind);
    if (inner == nullptr) return NULL;
    return reinterpret_cast<spiration_widget*>(new widget_handle(std::move(inner)));
}

void SPIRATION_CALL ui_widget_destroy(spiration_widget* handle) {
    widget_handle* box = as_handle(handle);
    if (box == nullptr) return;

    if (widget* parent_widget = box->parent()) {
        /* remove_child 返回的 unique_ptr 出作用域即析构。 */
        parent_widget->remove_child(box);
    } else {
        delete box;
    }
    repaint();
}

spiration_status SPIRATION_CALL ui_widget_append(spiration_widget* parent,
                                                 spiration_widget* child) {
    widget_handle* parent_handle = as_handle(parent);
    widget_handle* child_handle = as_handle(child);
    if (parent_handle == nullptr || child_handle == nullptr) {
        return SPIRATION_ERR_NOT_FOUND;
    }
    if (child_handle->parent() != nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT;
    }

    parent_handle->add_child(std::unique_ptr<widget>(child_handle));
    repaint();
    return SPIRATION_OK;
}

/** @brief 组装界面函数表。 */
spiration_ui_api make_ui_api() {
    spiration_ui_api api;
    std::memset(&api, 0, sizeof(api));

    api.version = SPIRATION_UI_VERSION;

    api.request_repaint = ui_request_repaint;
    api.add_menu_item = ui_add_menu_item;

    api.tab_open = ui_tab_open;
    api.tab_close = ui_tab_close;
    api.tab_activate = ui_tab_activate;
    api.tab_set_title = ui_tab_set_title;
    api.tab_set_content = ui_tab_set_content;
    api.tab_is_alive = ui_tab_is_alive;

    api.widget_create = ui_widget_create;
    api.widget_destroy = ui_widget_destroy;
    api.widget_append = ui_widget_append;
    api.widget_set = ui_widget_set;
    api.widget_get = ui_widget_get;
    api.widget_listen = ui_widget_listen;

    api.size = static_cast<uint32_t>(sizeof(api));
    return api;
}

} // namespace

void release_ui_owned(const spiration_extension_desc* owner) {
    if (owner == nullptr) return;

    /* 1. 关闭该拓展打开的标签页：控件树随之一并销毁，其中的回调自然失效。 */
    std::vector<owned_tab> closing;
    std::vector<owned_tab> keeping;
    for (size_t i = 0; i < owned_tabs().size(); ++i) {
        if (owned_tabs()[i].owner == owner) {
            closing.push_back(owned_tabs()[i]);
        } else {
            keeping.push_back(owned_tabs()[i]);
        }
    }
    owned_tabs().swap(keeping);

    for (size_t i = 0; i < closing.size(); ++i) {
        tab_handle* target = as_handle(reinterpret_cast<spiration_tab*>(closing[i].handle));
        if (target == nullptr) continue;

        root* root_widget = nullptr;
        tab_bar* strip = tab_strip(root_widget);
        if (strip == nullptr) continue;
        for (int index = 0; index < strip->tab_count(); ++index) {
            if (strip->get_tab(index) == target) {
                strip->close_tab(index);
                break;
            }
        }
    }

    /* 2. 移除该拓展添加的菜单项。 */
    root* root_widget = dynamic_cast<root*>(application::instance()->widget());
    if (root_widget != nullptr) {
        root_widget->remove_menu_items(text_of(owner->id));
    }
    repaint();
}

const spiration_ui_api* ui_interface(uint32_t required_version) {
    /* 主版本必须一致，且宿主实现不得低于请求的次版本。 */
    if (SPIRATION_API_VERSION_MAJOR_OF(required_version) !=
        SPIRATION_API_VERSION_MAJOR_OF(SPIRATION_UI_VERSION)) {
        return NULL;
    }
    if (required_version > SPIRATION_UI_VERSION) return NULL;

    static const spiration_ui_api api = make_ui_api();
    return &api;
}

} // namespace capi
} // namespace spiration
