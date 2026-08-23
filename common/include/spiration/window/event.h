/**
 * @file event.h
 * @brief 事件定义。
 * @author clk
 */

#pragma once

#include <tl/optional.hpp>
#include <spiration/ui/point.h>

namespace spiration {

/**
 * @brief 事件类型枚举。
 */
enum class event_type {
    mouse,
    keyboard,
    ime_composition,
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
    bool shift = false;  // Shift 是否按住（Shift+滚轮 → 水平滚动）
};


struct key_event_data {
    int key_code = 0;      
    unsigned int codepoint = 0; 
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool consumed = false;
};

/**
 * @brief 输入法组合文本（preedit）更新事件。
 * 各平台输入法（macOS NSTextInput、Windows IMM32、X11 XIM 等）
 * 在组合输入期间持续派发；空 text 表示组合结束（已提交/取消）。
 */
struct ime_composition_data {
    std::string text;  ///< 组合文本（UTF-8）；空串表示组合结束。
    int cursor = -1;   ///< 组合内光标位置（UTF-16 码元数）；-1 表示末尾。
};

}
