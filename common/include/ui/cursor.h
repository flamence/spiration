/**
 * @file cursor.h
 * @brief 鼠标光标管理。
 * @author clk
 */

#pragma once

namespace spiration {

class window;
class widget;

/**
 * @brief 鼠标光标形状。
 */
enum class cursor_type {
    default_cursor, ///< 默认
    text,           ///< 文本
    pointer,        ///< 手型
    crosshair,      ///< 十字
    move,           ///< 移动
    resize_h,       ///< 水平调整
    resize_v,       ///< 垂直调整
    resize_nwse,    ///< 右下、左上调整
    resize_nesw,    ///< 左下、右上调整
    forbidden,      ///< 禁止
};

/**
 * @brief 光标管理器单例。
 */
class cursor_manager {
public:
    static cursor_manager& instance();

    /**
     * @brief 绑定窗口。
     */
    void set_window(window* w);

    /**
     * @brief 根据根控件下最深层 hovered 控件的光标 style 属性更新光标。
     * @param root 根控件
     * @param x 鼠标在根控件局部坐标。
     * @param y 鼠标在根控件局部坐标。
     */
    void update(widget* root, float x, float y);

    /**
     * @brief 直接应用指定光标。
     */
    void apply(cursor_type c);

    cursor_type current() const { return current_; }

private:
    cursor_manager() = default;

    window* window_ = nullptr;
    cursor_type current_ = cursor_type::default_cursor;
};

} // namespace spiration
