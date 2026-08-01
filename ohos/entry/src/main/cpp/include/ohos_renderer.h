/**
 * @file ohos_renderer.h
 * @brief OHOS OpenGL ES 3.0 渲染器。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace spiration {

/**
 * @brief OHOS OpenGL ES 3.0 渲染器。
 */
class ohos_renderer : public renderer {
public:
    ohos_renderer();
    ~ohos_renderer() override;

    bool initialize(void* native_window_handle) override;
    void shutdown() override;
    void resize(uint32_t width, uint32_t height) override;

    void set_logical_size(uint32_t width, uint32_t height);

    void set_density(float d) { density_ = d; }

    void check_resize();

    float density_ = 1.0f;
    uint32_t last_egl_w_ = 0;
    uint32_t last_egl_h_ = 0;

    void get_viewport_size(uint32_t& width, uint32_t& height) const override;
    float measure_text_width(const std::string& text, float font_size,
                             const std::string& font_family) override;
    float measure_text_height(const std::string& text, float font_size = 16.0f,
                              const std::string& font_family = "Consolas",
                              float wrap_width = 10000.0f) override;

private:
    void begin_frame() override;
    void end_frame() override;
    void clear(const color& clear_color) override;

    void draw_rectangle(const rectangle& rect, const color& fill_color) override;
    void draw_rectangle_outline(const rectangle& rect, const color& stroke_color,
                                float stroke_width = 1.0f) override;
    void draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) override;
    void draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width = 1.0f) override;
    void push_clip(const rectangle& rect) override;
    void pop_clip() override;
    void draw_circle(const point& center, float radius, const color& fill_color) override;
    void draw_circle_outline(const point& center, float radius, const color& stroke_color,
                             float stroke_width = 1.0f) override;
    void draw_line(const point& start, const point& end, const color& stroke_color,
                   float stroke_width = 1.0f) override;

    void draw_text(const std::string& text, const point& position, const color& text_color,
                   float font_size = 16.0f, const std::string& font_family = "Arial",
                   bool word_wrap = true) override;
    void draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                           text_alignment h_align = text_alignment::left,
                           vertical_alignment v_align = vertical_alignment::top,
                           float font_size = 16.0f,
                           const std::string& font_family = "Arial") override;

    void draw_image(const std::string& image_path, const rectangle& destination) override;
    void draw_image_subregion(const std::string& image_path, const rectangle& source,
                              const rectangle& destination) override;

    void push_transform(float x, float y, float rotation = 0.0f,
                        float scale_x = 1.0f, float scale_y = 1.0f) override;
    void pop_transform() override;

    void set_blend_mode(bool enabled) override;
    void set_alpha(float alpha) override;

private:
    struct vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };
    static constexpr size_t MAX_BATCH_VERTS = 4096;

    EGLDisplay egl_display_ = EGL_NO_DISPLAY;
    EGLContext egl_context_ = EGL_NO_CONTEXT;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;
    EGLConfig egl_config_ = nullptr;

    uint32_t viewport_width_ = 0;
    uint32_t viewport_height_ = 0;
    uint32_t physical_width_ = 0;   // 物理像素宽度
    uint32_t physical_height_ = 0;  // 物理像素高度

    struct Transform {
        float m[16];
    };
    std::vector<Transform> transform_stack_;
    std::vector<rectangle> clip_stack_;
    Transform current_transform_;

    float proj_[16];
    float mvp_[16];

    float current_alpha_ = 1.0f;
    bool blend_enabled_ = true;

    GLuint shader_program_ = 0;
    GLint u_mvp_loc_ = -1;
    GLint u_color_loc_ = -1;
    GLint u_use_tex_loc_ = -1;
    GLint u_tex_loc_ = -1;

    GLuint vbo_ = 0;
    GLuint vao_ = 0;
    std::vector<vertex> batch_;
    bool batch_has_texture_ = false;

    struct image_resource {
        GLuint texture_id = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::unordered_map<std::string, image_resource> images_;

    bool init_egl(void* native_window);
    bool init_shaders();
    bool create_buffers();
    void destroy_buffers();
    void setup_projection();
    void apply_transform();
    void flush_batch();
    void transform_point(float& x, float& y) const;

    void push_rect_verts(float x, float y, float w, float h,
                         float r, float g, float b, float a);
    void push_tex_verts(float x, float y, float w, float h,
                        float u0, float v0, float u1, float v1,
                        float r, float g, float b, float a);

    image_resource* load_image(const std::string& path);

    static void mat4_identity(float* m);
    static void mat4_multiply(float* result, const float* a, const float* b);
    static void mat4_ortho(float* m, float left, float right, float bottom, float top);
    static void mat4_translate(float* m, float x, float y, float z);
    static void mat4_rotate_z(float* m, float angle_deg);
    static void mat4_scale(float* m, float sx, float sy, float sz);
};

} // namespace spiration
