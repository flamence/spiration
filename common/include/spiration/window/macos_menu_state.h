/**
 * @file macos_menu_state.h
 * @author clk
 * @brief 菜单项堆上状态：标题与回调存放于同一堆对象，
 *        桥接（macos_menu.mm）通过不透明指针读取，
 *        避免任何栈帧/临时对象生命周期问题。仅供 macOS 使用。
 */

#pragma once

#include <functional>
#include <string>

/**
 * @brief 单个菜单项的堆上状态。
 * 由调用方（root）在堆上创建，桥接强持有其回调指针；
 * 标题字符串位于堆内，与菜单项同生命周期（当前实现不释放）。
 */
struct spiration_menu_item_state {
    std::string title;               ///< 菜单项显示标题（UTF-8，堆上存储）。
    std::function<void()> callback;  ///< 点击回调。
};
