/**
 * @file opengl_renderer.h
 * @brief OpenGL 渲染器。
 * @author clk
 */

#pragma once

#include <spiration/renderer/renderer.h>

#if defined(__OHOS__)
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#if defined(__linux__) && !defined(__OHOS__)
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

#if defined(__linux__) && !defined(__OHOS__)
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
    /// 清空以字体面指针为键的字形缓存并重建字形图集（丢弃旧 UV）。
    /// 调用前会先提交当前批次，避免批次中的旧 UV 指向已删除的图集。
    void reset_glyph_caches();

    /// 按顺序查找首个可读的字体文件（公共实现；字体路径由各平台提供）。
    static std::string find_font_file(const char* const* paths);
#endif

    struct image_resource {
        GLuint texture_id = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    image_resource* load_image(const std::string& path);

    struct transform {
        float m[16]; // 4x4（仅使用左上 2x3 的仿射部分，其余固定为单位阵）
    };

    void transform_point(float& x, float& y) const;

    static void mat4_identity(float* m);
    static void mat4_ortho(float* m, float left, float right, float bottom, float top);

    /// 提交前保证批次容量：顶点数超过 MAX_BATCH_VERTICES 时先 flush。
    /// 必须在 push 之前调用，避免“先 push 再判断”导致单次绘制越过 VBO 容量。
    void ensure_batch_capacity(size_t extra_vertices);
    /// 提交前保证批次纹理一致：纹理切换（纯色/字形图集/图片）时先 flush。
    /// texture_id == 0 表示无纹理的纯色图元。
    void ensure_batch_texture(GLuint texture_id);

    /// 按半径自适应计算圆角/圆形的分段数，减少小半径图元的冗余顶点。
    static int corner_segment_count(float radius);
    static int circle_segment_count(float radius);

    void draw_rect_impl(float x, float y, float w, float h, const color& fill_color);
    void draw_rect_outline_impl(float x, float y, float w, float h, const color& stroke_color, float stroke_width);
    void draw_circle_impl(float cx, float cy, float radius, const color& fill_color, bool filled, float stroke_width);
    void apply_clip();

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    shader_program shape_shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::vector<vertex> batch_;

#if defined(__linux__) && !defined(__OHOS__)
    FT_Library ft_library_ = nullptr;
    std::unordered_map<std::string, font_face> font_faces_;
    std::unordered_map<uint64_t, glyph_info> glyph_cache_;
    std::unordered_map<uint64_t, float> glyph_advance_cache_;
    glyph_atlas glyph_atlas_;
    font_face* cjk_fallback_ = nullptr;
#endif

    std::unordered_map<std::string, image_resource> images_;

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

    /// 记录一次文本测量结果；缓存满时做有界淘汰，避免整表 clear() 造成周期性布局抖动。
    void cache_measure_result(measure_cache_key&& key, float value);

    std::vector<transform> transform_stack_;
    std::vector<rectangle> clip_stack_;
    transform current_transform_;
    float proj_matrix_[16];
    float mvp_matrix_[16];

    float alpha_ = 1.0f;
    bool blend_enabled_ = true;
    bool batch_has_texture_ = false;
    GLuint current_texture_ = 0;

    /// 批次提交时的 GL 状态缓存：program 与 MVP 在一帧内不变，避免每次 flush 重复上传。
    GLuint cached_program_ = 0;
    bool mvp_uploaded_ = false;

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
#elif defined(__APPLE__)
    /// NSOpenGLContext 句柄（由窗口创建并持有，渲染器仅使用）。
    void* gl_context_ = nullptr;
    bool init_gl_context(void* native_window);
    void destroy_gl_context();
#endif

    static constexpr int CIRCLE_SEGMENTS = 48;
    static constexpr int MAX_CORNER_SEGMENTS = 24;
    static constexpr int MAX_BATCH_VERTICES = 8192;
    static constexpr size_t MEASURE_CACHE_MAX = 16384;
    /// 测量缓存满时一次淘汰的条目数（约 1/8），比整表清空平滑。
    static constexpr size_t MEASURE_CACHE_EVICT = MEASURE_CACHE_MAX / 8;

#if defined(__linux__) && !defined(__OHOS__)
    /// 字形/字体面/度量缓存上限：避免长期运行下按字体、字号、字符组合无界增长。
    static constexpr size_t GLYPH_CACHE_MAX = 4096;
    static constexpr size_t GLYPH_ADVANCE_CACHE_MAX = 32768;
    static constexpr size_t FONT_FACE_MAX = 32;
#endif
};

} // namespace spiration
