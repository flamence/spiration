/**
 * @file renderer.h
 * @brief 跨平台 2D 渲染器抽象接口定义。
 * @author clk
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <ui/color.h>
#include <ui/rectangle.h>

namespace spiration {

/**
 * @brief 文本水平对齐方式。
 */
enum class text_alignment {
    left,
    center,
    right
};

enum class vertical_alignment {
    top,
    center,
    bottom
};

/**
 * @brief 2D 渲染器抽象基类。
 */
class renderer {
public:
    virtual ~renderer() = default;
    
    virtual bool initialize(void* native_window_handle) = 0;
    virtual void shutdown() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual void clear(const color& clear_color) = 0;
    
    virtual void draw_rectangle(const rectangle& rectangle, const color& fill_color) = 0;
    virtual void draw_rectangle_outline(const rectangle& rectangle, const color& stroke_color, float stroke_width = 1.0f) = 0;

    virtual void draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) {
        (void)radius;
        draw_rectangle(rect, fill_color);
    }
    virtual void draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width = 1.0f) {
        (void)radius;
        draw_rectangle_outline(rect, stroke_color, stroke_width);
    }

    virtual void push_clip(const rectangle& rect) { (void)rect; }
    virtual void pop_clip() {}
    virtual void draw_circle(const point& center, float radius, const color& fill_color) = 0;
    virtual void draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width = 1.0f) = 0;
    virtual void draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width = 1.0f) = 0;
    
    virtual void draw_text(const std::string& text, const point& position, const color& text_color, 
                          float font_size = 16.0f, const std::string& font_family = "Arial",
                          bool word_wrap = true) = 0;
    virtual void draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                                  text_alignment h_align = text_alignment::left,
                                  vertical_alignment v_align = vertical_alignment::top,
                                  float font_size = 16.0f, const std::string& font_family = "Arial") = 0;
    
    virtual void draw_image(const std::string& image_path, const rectangle& destination) = 0;
    virtual void draw_image_subregion(const std::string& image_path, const rectangle& source, const rectangle& destination) = 0;

    /// @brief 查询图片尺寸（可解码返回 true 并填充宽高）。默认不支持则返回 false。
    virtual bool query_image_size(const std::string& image_path, uint32_t& width, uint32_t& height) {
        (void)image_path; (void)width; (void)height;
        return false;
    }
    
    virtual void push_transform(float x, float y, float rotation = 0.0f, float scale_x = 1.0f, float scale_y = 1.0f) = 0;
    virtual void pop_transform() = 0;
    
    virtual void set_blend_mode(bool enabled) = 0;
    virtual void set_alpha(float alpha) = 0;
    
    virtual void get_viewport_size(uint32_t& width, uint32_t& height) const = 0;

    
    virtual float measure_text_width(const std::string& text, float font_size = 16.0f,
                                     const std::string& font_family = "Consolas") = 0;

    /// @brief 测量文本高度（DIP），考虑给定宽度内的自动换行。
    /// @param text  要测量的文本
    /// @param font_size  字体大小（DIP）
    /// @param font_family  字体名称
    /// @param wrap_width  换行宽度（DIP），超出此宽度自动换行
    /// @return 文本在给定宽度内完整渲染所需的高度（DIP）
    virtual float measure_text_height(const std::string& text, float font_size = 16.0f,
                                       const std::string& font_family = "Consolas",
                                       float wrap_width = 10000.0f) = 0;
    
    static std::shared_ptr<renderer> create_direct2d_renderer();
    static std::shared_ptr<renderer> create_opengl_renderer();
    static std::shared_ptr<renderer> create_metal_renderer();
    static std::shared_ptr<renderer> create_vulkan_renderer();
};

}