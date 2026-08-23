/**
 * @file macos_menu.h
 * @author clk
 * @brief macOS 原生菜单桥接声明（实现见 macos/src/spiration/window/macos_menu.mm）。
 *        仅供 macOS 使用；其余平台不引用此头文件。
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 菜单操作参数。
 * 统一通过单个结构体指针传递，避免多参数 C 桥接在部分构建配置下
 * 丢失第二个及之后参数的问题。
 */
typedef struct spiration_macos_menu_spec {
    void* submenu;      ///< add_item：目标子菜单句柄；get_submenu 忽略。
    const char* key;    ///< get_submenu：菜单标识（如 "menu.file"）。
    const char* title;  ///< get_submenu：菜单本地化显示标题；add_item 忽略
                        ///< （菜单项标题由堆上状态对象提供）。
    void* callback;     ///< add_item：堆上 spiration_menu_item_state*（含标题与回调）。
} spiration_macos_menu_spec;

/**
 * @brief 获取（或创建）主菜单栏下以 key 标识的子菜单。
 * @param spec key/title 必填。
 * @return 子菜单句柄（NSMenu*），失败返回 NULL。
 */
void* spiration_macos_menu_get_submenu(const spiration_macos_menu_spec* spec);

/**
 * @brief 在子菜单末尾添加一个菜单项。
 * @param spec submenu/title/callback 必填。
 * @return 1 成功，0 失败。
 */
int spiration_macos_menu_add_item(const spiration_macos_menu_spec* spec);

#ifdef __cplusplus
} // extern "C"
#endif
