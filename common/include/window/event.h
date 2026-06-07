/**
 * @file event.h
 * @brief 事件系统类型定义（事件类型、鼠标事件数据等）。
 * @author clk
 */

#pragma once

#include <ui/point.h>

namespace spiration {

/**
 * @brief 事件类型枚举。
 */
enum class event_type {
    mouse,
    keyboard,
    window_resize,
};

struct event_data {};

enum class mouse_button {
    none,
    left,
    right,
    middle
};

enum class mouse_action {
    up,
    down,
    wheel,
    move,
};

struct mouse_event_data {
    point position;
    mouse_button button = mouse_button::none;
    mouse_action action = mouse_action::move;
    bool consumed = false;
    int wheel_delta = 0;
};


struct key_event_data {
    int key_code = 0;      
    unsigned int codepoint = 0; 
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool consumed = false;
    float ime_x = -1, ime_y = -1; 
};

}