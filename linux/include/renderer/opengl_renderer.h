/**
 * @file opengl_renderer.h
 * @brief OpenGL 3.3 Core Profile 渲染器实现。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace spiration {

/**
 * @brief OpenGL 3.3 Core Profile 渲染器实现。
 */
class opengl_renderer : public renderer {
public:
    opengl_renderer();
    ~opengl_renderer() override;

    opengl_renderer(const opengl_renderer&) = delete;
    opengl_renderer& operator=(const opengl_renderer&) = delete;

    opengl_renderer(opengl_renderer&& other) noexcept;
    opengl_renderer& operator=(opengl_renderer&& other) noexcept;

    bool initialize(void* native_window_handle) override;
    void shutdown() override;
    void resize(uint32_t width, uint32_t height) override;

    void begin_frame() override;
    void end_frame() override;
    void clear(const color& clear_color) override;

    void draw_rectangle(const rectangle& rectangle, const color& fill_color) override;
    void draw_rectangle_outline(const rectangle& rectangle, const color& stroke_color, float stroke_width = 1.0f) override;
    void draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) override;
    void draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width = 1.0f) override;
    void push_clip(const rectangle& rect) override;
    void pop_clip() override;
    void draw_circle(const point& center, float radius, const color& fill_color) override;
    void draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width = 1.0f) override;
    void draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width = 1.0f) override;

    void draw_text(const std::string& text, const point& position, const color& text_color,
                   float font_size = 16.0f, const std::string& font_family = "Arial",
                   bool word_wrap = true) override;
    void draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                           text_alignment h_align = text_alignment::left,
                           vertical_alignment v_align = vertical_alignment::top,
                           float font_size = 16.0f, const std::string& font_family = "Arial") override;

    void draw_image(const std::string& image_path, const rectangle& destination) override;
    void draw_image_subregion(const std::string& image_path, const rectangle& source, const rectangle& destination) override;

    bool query_image_size(const std::string& image_path, uint32_t& width, uint32_t& height) override;

    void push_transform(float x, float y, float rotation = 0.0f, float scale_x = 1.0f, float scale_y = 1.0f) override;
    void pop_transform() override;

    void set_blend_mode(bool enabled) override;
    void set_alpha(float alpha) override;

    void get_viewport_size(uint32_t& width, uint32_t& height) const override;

    float measure_text_width(const std::string& text, float font_size = 16.0f,
                             const std::string& font_family = "Consolas") override;
    float measure_text_height(const std::string& text, float font_size = 16.0f,
                              const std::string& font_family = "Consolas",
                              float wrap_width = 10000.0f) override;

    struct vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    bool create_buffers();
    void destroy_buffers();
    void flush_batch();

private:
    struct shader_program {
        GLuint program = 0;
        GLint u_mvp = -1;
        GLint u_color = -1;
        GLint u_use_texture = -1;
        GLint u_texture_sampler = -1;
    };

    bool compile_shaders();
    void destroy_shaders();

    struct glyph_info {
        float s0, t0, s1, t1;
        float bearing_x, bearing_y;
        float advance_x;
        float width, height;
    };

    struct font_face {
        FT_Face face = nullptr;
        float size = 0.0f;
    };

    struct glyph_atlas {
        GLuint texture_id = 0;
        int width = 1024;
        int height = 1024;
        int row_height = 0;
        int cursor_x = 0;
        int cursor_y = 0;
    };

    bool init_freetype();
    bool init_cjk_fallback_font();
    font_face* get_font_face(const std::string& family, float size);
    glyph_info* get_glyph(font_face* face, char32_t codepoint);
    bool init_glyph_atlas(glyph_atlas* atlas);

    struct image_resource {
        GLuint texture_id = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    image_resource* load_image(const std::string& path);

    struct transform {
        float m[16]; // 4x4
    };

    void apply_transform();

    void transform_point(float& x, float& y) const;

    static void mat4_identity(float* m);
    static void mat4_multiply(float* result, const float* a, const float* b);
    static void mat4_ortho(float* m, float left, float right, float bottom, float top);
    static void mat4_translate(float* m, float x, float y, float z);
    static void mat4_rotate_z(float* m, float angle_deg);
    static void mat4_scale(float* m, float sx, float sy, float sz);

    void draw_rect_impl(float x, float y, float w, float h, const color& fill_color);
    void draw_rect_outline_impl(float x, float y, float w, float h, const color& stroke_color, float stroke_width);
    void draw_circle_impl(float cx, float cy, float radius, const color& fill_color, bool filled);
    void apply_clip();

private:
    Display* display_ = nullptr;

    uint32_t width_ = 0;
    uint32_t height_ = 0;

    shader_program shape_shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::vector<vertex> batch_;

    FT_Library ft_library_ = nullptr;
    std::unordered_map<std::string, font_face> font_faces_;
    std::unordered_map<uint64_t, glyph_info> glyph_cache_;
    glyph_atlas glyph_atlas_;
    font_face* cjk_fallback_ = nullptr;

    std::unordered_map<std::string, image_resource> images_;

    std::vector<transform> transform_stack_;
    std::vector<rectangle> clip_stack_;
    transform current_transform_;
    float proj_matrix_[16];
    float mvp_matrix_[16];

    float alpha_ = 1.0f;
    bool blend_enabled_ = true;
    bool batch_has_texture_ = false;
    GLuint current_texture_ = 0;

    static constexpr int CIRCLE_SEGMENTS = 48;
    static constexpr int MAX_BATCH_VERTICES = 8192;
};

} // namespace spiration
