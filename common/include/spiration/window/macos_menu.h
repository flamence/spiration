/**
 * @file macos_menu.h
 * @author clk
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取主菜单栏下以 key 标识的子菜单。
 * @param key 菜单标识，用于跨调用匹配同一菜单
 * @param title 本地化后的显示标题
 * @return 子菜单句柄，失败返回 NULL。
 */
void* spiration_macos_menu_get_submenu(const char* key, const char* title);

/**
 * @brief 在子菜单末尾添加一个菜单项。
 * @param submenu 获取子菜单时返回的句柄
 * @param title 菜单项显示文本
 * @param callback 菜单回调
 * @return 1 成功，0 失败。
 */
int spiration_macos_menu_add_item(void* submenu, const char* title, void* callback);

#ifdef __cplusplus
} // extern "C"
#endif
