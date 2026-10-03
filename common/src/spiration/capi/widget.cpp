/**
 * @file widget.cpp
 * @brief `spiration.ui` · 控件（创建、属性读写、事件订阅）。
 * @author clk
 *
 * 属性与事件都以字符串键暴露，未知键返回 `SPIRATION_ERR_UNSUPPORTED`，
 * 因此后续版本可以在不破坏 ABI 的前提下增加新键。
 */

#include "internal.h"
#include "internal_ui.h"

#include <spiration/ui/button.h>
#include <spiration/ui/checkbox.h>
#include <spiration/ui/container.h>
#include <spiration/ui/label.h>
#include <spiration/ui/layout.h>
#include <spiration/ui/progress_bar.h>
#include <spiration/ui/slider.h>
#include <spiration/ui/spacer.h>
#include <spiration/ui/separator.h>
#include <spiration/ui/text_area.h>
#include <spiration/ui/text_field.h>

#include <cstring>
#include <memory>
#include <string>

namespace spiration {
namespace capi {

namespace {

/* -------------------------------------------------------------------------
 * 属性
 * ---------------------------------------------------------------------- */

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

} // namespace

void emit_widget_event(spiration_widget_callback callback, void* user_data,
                       widget_handle* self, const char* event,
                       spiration_value data) {
    if (callback == nullptr) return;
    callback(user_data, reinterpret_cast<spiration_widget*>(self),
             spiration_str_view_of(event, std::strlen(event)), data);
}

/* -------------------------------------------------------------------------
 * 生命周期
 * ---------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * 属性
 * ---------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * 事件
 * ---------------------------------------------------------------------- */

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

} // namespace capi
} // namespace spiration
