/**
 * @file opengl_renderer.h
 * @brief OpenGL 渲染器。
 * @author clk
 *
 * @section platform 双平台架构说明
 *
 * 本文件由 Linux 与 OpenHarmony(OHOS) 两个平台编译，通过 `#if defined(__OHOS__)`
 * 在同一份源码中维护两条差异较大的实现路径（Windows 使用 D2D、macOS 使用 Metal，
 * 不编译本文件）：
 *
 * - **Linux**（`__OHOS__` 未定义）：GL 3.3 Core + GLX（上下文在 x11_window 创建），
 *   文本走 FreeType 字形图集（`get_font_face`/`get_glyph`/`get_glyph_advance`）。
 * - **OHOS**（`__OHOS__` 定义）：GLES 3.0 + EGL（`init_egl`），文本走 native_drawing
 *   （`OH_Drawing_Typography` 排版后光栅化上传纹理），并额外暴露 DPI/物理尺寸 API
 *   （`set_density` 等）。
 *
 * 共享部分（顶点批处理、着色器、矩阵、裁剪、变换、stb 图像）与平台特定部分
 * （着色器源码、文本管线、EGL 上下文、OHOS 尺寸 API）在同一文件中以条件编译共存，
 * 约半数为平台分支代码。
 *
 * @warning 维护注意：修改共享逻辑必须同时考虑两条路径；修改平台分支只能在该平台
 * 上验证。代码拆分（文本管线分文件、共享核心保留）需在 Linux/OHOS 环境执行并回归，
 * 本机（Windows）无法编译验证。
 */

#pragma once

#include <renderer/renderer.h>

#if defined(__OHOS__)
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#if !defined(__OHOS__)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace spiration {

/**
 * @brief OpenGL 3.3 Core Profile 渲染器。
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

#if defined(__OHOS__)
    void set_density(float density);
    void set_physical_size(uint32_t width, uint32_t height);
    void set_logical_size(uint32_t width, uint32_t height);
    void check_resize();
#endif

    float measure_text_width(const std::string& text, float font_size = 16.0f,
                             const std::string& font_family = "Arial") override;
    float measure_text_height(const std::string& text, float font_size = 16.0f,
                              const std::string& font_family = "Arial",
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

#if !defined(__OHOS__)
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
    float get_glyph_advance(font_face* face, char32_t codepoint);
    bool init_glyph_atlas(glyph_atlas* atlas);
#endif

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
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    shader_program shape_shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::vector<vertex> batch_;

#if !defined(__OHOS__)
    FT_Library ft_library_ = nullptr;
    std::unordered_map<std::string, font_face> font_faces_;
    std::unordered_map<uint64_t, glyph_info> glyph_cache_;
    std::unordered_map<uint64_t, float> glyph_advance_cache_;
    glyph_atlas glyph_atlas_;
    font_face* cjk_fallback_ = nullptr;
#endif

    std::unordered_map<std::string, image_resource> images_;

    /// 文本测量缓存：避免布局/重排阶段高频重复测量同一文本。
    struct measure_cache_key {
        std::string text;
        std::string family;
        float size = 0.0f;
        float wrap = 0.0f;
        bool operator==(const measure_cache_key& o) const {
            return size == o.size && wrap == o.wrap &&
                   text == o.text && family == o.family;
        }
    };
    struct measure_cache_hash {
        size_t operator()(const measure_cache_key& k) const {
            size_t h = std::hash<std::string>{}(k.text);
            h ^= std::hash<float>{}(k.size) + 0x9e3779b9u + (h << 6) + (h >> 2);
            h ^= std::hash<float>{}(k.wrap) + 0x9e3779b9u + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>{}(k.family) + 0x9e3779b9u + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<measure_cache_key, float, measure_cache_hash> measure_cache_;

    std::vector<transform> transform_stack_;
    std::vector<rectangle> clip_stack_;
    transform current_transform_;
    float proj_matrix_[16];
    float mvp_matrix_[16];

    float alpha_ = 1.0f;
    bool blend_enabled_ = true;
    bool batch_has_texture_ = false;
    GLuint current_texture_ = 0;

#if defined(__OHOS__)
    float density_ = 1.0f;
    uint32_t physical_width_ = 0;
    uint32_t physical_height_ = 0;
    EGLDisplay egl_display_ = EGL_NO_DISPLAY;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;
    EGLContext egl_context_ = EGL_NO_CONTEXT;
    EGLConfig egl_config_ = nullptr;
    bool init_egl(void* native_window);
    void destroy_egl();
#endif

    static constexpr int CIRCLE_SEGMENTS = 48;
    static constexpr int MAX_BATCH_VERTICES = 8192;
    static constexpr size_t MEASURE_CACHE_MAX = 16384;
};

} // namespace spiration
