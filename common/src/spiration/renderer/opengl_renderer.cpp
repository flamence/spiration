/**
 * @file opengl_renderer.cpp
 * @brief OpenGL 渲染器实现。
 * @author clk
 */

#if defined(__linux__) || defined(__OHOS__) || defined(__APPLE__)
#include <spiration/application.h>
#include <spiration/renderer/opengl_renderer.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fmt/format.h>

#if defined(__APPLE__)
#include <spiration/renderer/opengl_platform.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


#if defined(__OHOS__)
static const char* SHAPE_VERTEX_SOURCE = R"glsl(
#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

static const char* SHAPE_FRAGMENT_SOURCE = R"glsl(
#version 300 es
precision mediump float;
in vec2 vTexCoord;
in vec4 vColor;

uniform bool uUseTexture;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    if (uUseTexture) {
        vec4 texel = texture(uTexture, vTexCoord);
        FragColor = texel * vColor;
    } else {
        FragColor = vColor;
    }
}
)glsl";
#else
static const char* SHAPE_VERTEX_SOURCE = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

static const char* SHAPE_FRAGMENT_SOURCE = R"glsl(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

uniform bool uUseTexture;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    if (uUseTexture) {
        vec4 texel = texture(uTexture, vTexCoord);
        FragColor = texel * vColor;
    } else {
        FragColor = vColor;
    }
}
)glsl";
#endif

namespace spiration {

namespace {

inline void push_vertex(std::vector<opengl_renderer::vertex>& batch,
                        float x, float y, float u, float v,
                        float r, float g, float b, float a) {
    batch.push_back({x, y, u, v, r, g, b, a});
}

inline void push_rect_vertices(std::vector<opengl_renderer::vertex>& batch,
                                float x, float y, float w, float h,
                                float u0, float v0, float u1, float v1,
                                float r, float g, float ba, float a) {
    push_vertex(batch, x,     y,     u0, v0, r, g, ba, a);
    push_vertex(batch, x + w, y,     u1, v0, r, g, ba, a);
    push_vertex(batch, x,     y + h, u0, v1, r, g, ba, a);
    push_vertex(batch, x + w, y,     u1, v0, r, g, ba, a);
    push_vertex(batch, x + w, y + h, u1, v1, r, g, ba, a);
    push_vertex(batch, x,     y + h, u0, v1, r, g, ba, a);
}

GLuint compile_shader_source(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        application::instance()->logger()->log({logging::level::error, fmt::format("OpenGL shader compilation failed ({}): {}", type, info), "renderer/opengl"});
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint link_shader_program(GLuint vs, GLuint fs) {
    GLuint program = glCreateProgram();
    if (!program) return 0;
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char info[512];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        application::instance()->logger()->log({logging::level::error, fmt::format("OpenGL program link failed: {}", info), "renderer/opengl"});
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

}

void opengl_renderer::mat4_identity(float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void opengl_renderer::mat4_ortho(float* m, float left, float right, float bottom, float top) {
    mat4_identity(m);
    m[0]  = 2.0f / (right - left);
    m[5]  = 2.0f / (top - bottom);
    m[10] = -1.0f;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
}

void opengl_renderer::ensure_batch_capacity(size_t extra_vertices) {
    // 必须在写入顶点之前调用：单个图元一次可能产生数百个顶点（例如圆角矩形），
    // 若像旧实现那样“先 push 再判断”，批次数会超过 MAX_BATCH_VERTICES，
    // 进而让 flush_batch 的 glBufferSubData 写入超过 VBO 容量（GL_INVALID_VALUE，整批丢失）。
    if (batch_.size() + extra_vertices > static_cast<size_t>(MAX_BATCH_VERTICES)) {
        flush_batch();
    }
}

void opengl_renderer::ensure_batch_texture(GLuint texture_id) {
    // 一个批次只能用一种纹理：纹理切换（纯色图元 <-> 字形图集 <-> 图片）时先提交。
    // 这样既保证绘制顺序，也避免用错误的纹理采样。
    const bool textured = texture_id != 0;
    if (batch_.empty()) {
        batch_has_texture_ = textured;
        current_texture_ = texture_id;
        return;
    }
    if (batch_has_texture_ == textured && (!textured || current_texture_ == texture_id)) {
        return;
    }
    flush_batch();
    batch_has_texture_ = textured;
    current_texture_ = texture_id;
}

int opengl_renderer::corner_segment_count(float radius) {
    // 每 1/4 圆按半径决定段数：8px 圆角只需 4 段，弦高误差 < 0.2px，视觉上不可分辨。
    if (!(radius > 0.0f)) return 3;
    int segments = static_cast<int>(radius * 0.5f);
    if (segments < 3) segments = 3;
    if (segments > MAX_CORNER_SEGMENTS) segments = MAX_CORNER_SEGMENTS;
    return segments;
}

int opengl_renderer::circle_segment_count(float radius) {
    // 圆形同样按半径自适应；小图标半径往往只有 2~8，固定 48 段会浪费大量顶点。
    if (!(radius > 0.0f)) return 12;
    int segments = static_cast<int>(radius * 1.5f);
    if (segments < 12) segments = 12;
    if (segments > CIRCLE_SEGMENTS) segments = CIRCLE_SEGMENTS;
    return segments;
}

void opengl_renderer::cache_measure_result(measure_cache_key&& key, float value) {
    // 旧实现缓存满时直接 clear()，会在长时间运行/大量不同文本时周期性丢失整张缓存，
    // 造成布局阶段反复重新测量（卡顿抖动）。这里改为有界淘汰少量条目。
    if (measure_cache_.size() >= MEASURE_CACHE_MAX) {
        size_t evict = MEASURE_CACHE_EVICT;
        while (evict > 0 && !measure_cache_.empty()) {
            measure_cache_.erase(measure_cache_.begin());
            --evict;
        }
    }
    measure_cache_.emplace(std::move(key), value);
}

opengl_renderer::opengl_renderer() {
    mat4_identity(current_transform_.m);
    mat4_identity(proj_matrix_);
    mat4_identity(mvp_matrix_);
    // 只在这里预留一次：begin_frame() 每帧 reserve 会带来无谓的容量检查/分配抖动。
    batch_.reserve(1024);
}

opengl_renderer::~opengl_renderer() {
    shutdown();
}

opengl_renderer::opengl_renderer(opengl_renderer&& other) noexcept
    : width_(other.width_)
    , height_(other.height_)
    , shape_shader_(other.shape_shader_)
    , vao_(other.vao_)
    , vbo_(other.vbo_)
    , batch_(std::move(other.batch_))
#if defined(__linux__) && !defined(__OHOS__)
    , ft_library_(other.ft_library_)
    , font_faces_(std::move(other.font_faces_))
    , glyph_cache_(std::move(other.glyph_cache_))
    , glyph_advance_cache_(std::move(other.glyph_advance_cache_))
    , glyph_atlas_(other.glyph_atlas_)
    , cjk_fallback_(other.cjk_fallback_)
#endif
    , images_(std::move(other.images_))
    , measure_cache_(std::move(other.measure_cache_))
    , transform_stack_(std::move(other.transform_stack_))
    , clip_stack_(std::move(other.clip_stack_))
    , current_transform_(other.current_transform_)
    , alpha_(other.alpha_)
    , blend_enabled_(other.blend_enabled_)
    , batch_has_texture_(other.batch_has_texture_)
    , current_texture_(other.current_texture_)
#if defined(__APPLE__)
    , gl_context_(other.gl_context_)
#endif
{

    std::memcpy(proj_matrix_, other.proj_matrix_, sizeof(proj_matrix_));
    std::memcpy(mvp_matrix_, other.mvp_matrix_, sizeof(mvp_matrix_));

    other.width_ = 0;
    other.height_ = 0;
    other.shape_shader_ = {};
    other.vao_ = 0;
    other.vbo_ = 0;
#if defined(__linux__) && !defined(__OHOS__)
    other.ft_library_ = nullptr;
    other.glyph_atlas_ = {};
    other.cjk_fallback_ = nullptr;
#endif
    other.alpha_ = 1.0f;
    other.blend_enabled_ = true;
    other.batch_has_texture_ = false;
    other.current_texture_ = 0;
    other.cached_program_ = 0;
    other.mvp_uploaded_ = false;
#if defined(__APPLE__)
    other.gl_context_ = nullptr;
#endif
    mat4_identity(other.current_transform_.m);
}

opengl_renderer& opengl_renderer::operator=(opengl_renderer&& other) noexcept {
    if (this != &other) {
        shutdown();

        width_ = other.width_;
        height_ = other.height_;
        shape_shader_ = other.shape_shader_;
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        batch_ = std::move(other.batch_);
#if defined(__linux__) && !defined(__OHOS__)
        ft_library_ = other.ft_library_;
        font_faces_ = std::move(other.font_faces_);
        glyph_cache_ = std::move(other.glyph_cache_);
        glyph_advance_cache_ = std::move(other.glyph_advance_cache_);
        glyph_atlas_ = other.glyph_atlas_;
        cjk_fallback_ = other.cjk_fallback_;
#endif
        images_ = std::move(other.images_);
        measure_cache_ = std::move(other.measure_cache_);
        transform_stack_ = std::move(other.transform_stack_);
        clip_stack_ = std::move(other.clip_stack_);
        current_transform_ = other.current_transform_;
        alpha_ = other.alpha_;
        blend_enabled_ = other.blend_enabled_;
        batch_has_texture_ = other.batch_has_texture_;
        current_texture_ = other.current_texture_;
        cached_program_ = 0;
        mvp_uploaded_ = false;

        std::memcpy(proj_matrix_, other.proj_matrix_, sizeof(proj_matrix_));
        std::memcpy(mvp_matrix_, other.mvp_matrix_, sizeof(mvp_matrix_));

        other.width_ = 0;
        other.height_ = 0;
        other.shape_shader_ = {};
        other.vao_ = 0;
        other.vbo_ = 0;
#if defined(__linux__) && !defined(__OHOS__)
        other.ft_library_ = nullptr;
        other.glyph_atlas_ = {};
        other.cjk_fallback_ = nullptr;
#endif
        other.alpha_ = 1.0f;
        other.blend_enabled_ = true;
        other.batch_has_texture_ = false;
        other.current_texture_ = 0;
        other.cached_program_ = 0;
        other.mvp_uploaded_ = false;
#if defined(__APPLE__)
        other.gl_context_ = nullptr;
#endif
        mat4_identity(other.current_transform_.m);
    }
    return *this;
}

bool opengl_renderer::initialize(void* native_window_handle) {
    if (!native_window_handle) {
        application::instance()->logger()->log({logging::level::error, "invalid native window handle", "renderer/opengl"});
        return false;
    }

    width_ = 800;
    height_ = 600;

#if defined(__OHOS__)
    if (!init_egl(native_window_handle)) {
        application::instance()->logger()->log({logging::level::error, "EGL init failed", "renderer/opengl"});
        return false;
    }
#elif defined(__APPLE__)
    if (!init_gl_context(native_window_handle)) {
        application::instance()->logger()->log({logging::level::error, "OpenGL context init failed", "renderer/opengl"});
        return false;
    }
#endif

    if (!compile_shaders()) {
        application::instance()->logger()->log({logging::level::error, "failed to compile shaders", "renderer/opengl"});
        return false;
    }

    if (!create_buffers()) {
        application::instance()->logger()->log({logging::level::error, "failed to create buffers", "renderer/opengl"});
        return false;
    }

#if defined(__linux__) && !defined(__OHOS__)
    if (!init_freetype()) {
        application::instance()->logger()->log({logging::level::warning, "FreeType init failed, text rendering disabled", "renderer/opengl"});
    } else {
        init_cjk_fallback_font();
    }
    init_glyph_atlas(&glyph_atlas_);
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4_ortho(proj_matrix_, 0.0f, static_cast<float>(width_),
               static_cast<float>(height_), 0.0f);

    application::instance()->logger()->log({logging::level::info, fmt::format("initialized ({}x{})", width_, height_), "renderer/opengl"});
    return true;
}

void opengl_renderer::shutdown() {
    if (!batch_.empty()) {
        flush_batch();
    }

    for (auto& img_entry : images_) {
        if (img_entry.second.texture_id) glDeleteTextures(1, &img_entry.second.texture_id);
    }
    images_.clear();

#if defined(__linux__) && !defined(__OHOS__)
    if (glyph_atlas_.texture_id) {
        glDeleteTextures(1, &glyph_atlas_.texture_id);
        glyph_atlas_.texture_id = 0;
    }

    if (cjk_fallback_ && cjk_fallback_->face) {
        FT_Done_Face(cjk_fallback_->face);
        delete cjk_fallback_;
        cjk_fallback_ = nullptr;
    }
    for (auto& face_entry : font_faces_) {
        if (face_entry.second.face) FT_Done_Face(face_entry.second.face);
    }
    font_faces_.clear();
    glyph_cache_.clear();
    glyph_advance_cache_.clear();
    measure_cache_.clear();

    if (ft_library_) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }
#endif

    destroy_buffers();
    destroy_shaders();

#if defined(__OHOS__)
    destroy_egl();
#elif defined(__APPLE__)
    destroy_gl_context();
#endif

    application::instance()->logger()->log({logging::level::info, "shutdown", "renderer/opengl"});
}

void opengl_renderer::resize(uint32_t width, uint32_t height) {
    if (width_ == width && height_ == height) return;
    width_ = width;
    height_ = height;
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    mat4_ortho(proj_matrix_, 0.0f, static_cast<float>(width),
               static_cast<float>(height_), 0.0f);
    // mvp 在变换下推到 CPU（transform_point），内容恒等于投影矩阵；resize 后同步并让缓存失效。
    std::memcpy(mvp_matrix_, proj_matrix_, sizeof(mvp_matrix_));
    mvp_uploaded_ = false;
}

void opengl_renderer::begin_frame() {
    // batch_.clear() 保留容量（构造函数中已一次性 reserve），避免每帧 reserve 的抖动。
    batch_.clear();
    batch_has_texture_ = false;
    current_texture_ = 0;

    transform_stack_.clear();
    mat4_identity(current_transform_.m);
    std::memcpy(mvp_matrix_, proj_matrix_, sizeof(mvp_matrix_));
    mvp_uploaded_ = false;

    // 裁剪状态不跨帧：若上一帧 push_clip/pop_clip 未配对，残留的 scissor 会把本帧
    // （包括 glClear）全部裁掉，表现为整屏黑屏/闪烁。这里强制复位。
    clip_stack_.clear();
    glDisable(GL_SCISSOR_TEST);

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void opengl_renderer::end_frame() {
    if (!batch_.empty()) {
        flush_batch();
    }
#if defined(__OHOS__)
    eglSwapBuffers(egl_display_, egl_surface_);
#elif defined(__APPLE__)
    spiration_gl_flush(gl_context_);
#endif
}

void opengl_renderer::clear(const color& clear_color) {
    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void opengl_renderer::draw_rectangle(const rectangle& rect, const color& fill_color) {
    draw_rect_impl(rect.x, rect.y, rect.width, rect.height, fill_color);
}

void opengl_renderer::draw_rectangle_outline(const rectangle& rect, const color& stroke_color, float stroke_width) {
    draw_rect_outline_impl(rect.x, rect.y, rect.width, rect.height, stroke_color, stroke_width);
}

void opengl_renderer::draw_circle(const point& center, float radius, const color& fill_color) {
    draw_circle_impl(center.x, center.y, radius, fill_color, true, 0.0f);
}

void opengl_renderer::draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width) {
    draw_circle_impl(center.x, center.y, radius, stroke_color, false, stroke_width);
}

void opengl_renderer::draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width) {
    ensure_batch_texture(0);
    ensure_batch_capacity(6);

    float sx = start.x, sy = start.y;
    float ex = end.x, ey = end.y;
    transform_point(sx, sy);
    transform_point(ex, ey);

    float dx = ex - sx;
    float dy = ey - sy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = -dy / len * stroke_width * 0.5f;
    float ny =  dx / len * stroke_width * 0.5f;

    float ax = sx + nx, ay = sy + ny;
    float bx = sx - nx, by = sy - ny;
    float cx = ex - nx,   cy = ey - ny;
    float dx2 = ex + nx,  dy2 = ey + ny;

    float r = stroke_color.r, g = stroke_color.g, b = stroke_color.b, a = stroke_color.a * alpha_;

    push_vertex(batch_, ax, ay, 0, 0, r, g, b, a);
    push_vertex(batch_, bx, by, 0, 0, r, g, b, a);
    push_vertex(batch_, cx, cy, 0, 0, r, g, b, a);
    push_vertex(batch_, ax, ay, 0, 0, r, g, b, a);
    push_vertex(batch_, cx, cy, 0, 0, r, g, b, a);
    push_vertex(batch_, dx2, dy2, 0, 0, r, g, b, a);
}


void opengl_renderer::draw_image(const std::string& image_path, const rectangle& destination) {
    image_resource* img = load_image(image_path);
    if (!img || !img->texture_id) return;

    // 旧实现绕过批次直接把 6 个顶点写进 VBO 并立刻 draw，导致图片先于批次中已积累的
    // 图元提交（绘制顺序错乱），且完全忽略 push_transform（滚动容器中的图片不会滚动）。
    // 现在统一纳入批次，并按纹理分批。
    ensure_batch_texture(img->texture_id);
    ensure_batch_capacity(6);

    float x1 = destination.x, y1 = destination.y;
    float x2 = destination.x + destination.width;
    float y2 = destination.y + destination.height;
    transform_point(x1, y1);
    transform_point(x2, y2);

    const float a = alpha_;
    push_vertex(batch_, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x2, y1, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x1, y2, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x2, y1, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x2, y2, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x1, y2, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, a);
}

void opengl_renderer::draw_image_subregion(const std::string& image_path, const rectangle& source, const rectangle& destination) {
    image_resource* img = load_image(image_path);
    if (!img || !img->texture_id) return;

    float img_w = static_cast<float>(img->width);
    float img_h = static_cast<float>(img->height);
    if (!(img_w > 0.0f) || !(img_h > 0.0f)) return;

    float u0 = source.x / img_w;
    float v0 = source.y / img_h;
    float u1 = (source.x + source.width) / img_w;
    float v1 = (source.y + source.height) / img_h;

    // 与 draw_image 相同：纳入批次、按纹理分批、应用变换，避免顺序错乱和忽略变换。
    ensure_batch_texture(img->texture_id);
    ensure_batch_capacity(6);

    float x1 = destination.x, y1 = destination.y;
    float x2 = destination.x + destination.width;
    float y2 = destination.y + destination.height;
    transform_point(x1, y1);
    transform_point(x2, y2);

    const float a = alpha_;
    push_vertex(batch_, x1, y1, u0, v0, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x2, y1, u1, v0, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x1, y2, u0, v1, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x2, y1, u1, v0, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x2, y2, u1, v1, 1.0f, 1.0f, 1.0f, a);
    push_vertex(batch_, x1, y2, u0, v1, 1.0f, 1.0f, 1.0f, a);
}

bool opengl_renderer::query_image_size(const std::string& image_path, uint32_t& width, uint32_t& height) {
    image_resource* img = load_image(image_path);
    if (!img) return false;
    width = img->width;
    height = img->height;
    return true;
}

void opengl_renderer::push_transform(float x, float y, float rotation, float scale_x, float scale_y) {
    transform_stack_.push_back(current_transform_);

    // 只累积 2D 仿射参数（新变换 = 父变换 * 局部变换，与旧 mat4_multiply(local, current)
    // 的结果等价），避免每压栈一次就做 16x16 矩阵乘法。
    // 局部矩阵 local = T(x, y) * R(rotation) * S(scale_x, scale_y)。
    // 注意：c 与 m 指向同一块存储，必须先把旧的 2x3 分量取到局部变量，否则会边写边读。
    const float* c = current_transform_.m;
    const float c00 = c[0], c10 = c[1];
    const float c01 = c[4], c11 = c[5];
    const float c03 = c[12], c13 = c[13];

    float rad = rotation * 3.14159265f / 180.0f;
    float cos_r = std::cos(rad);
    float sin_r = std::sin(rad);

    const float l00 = scale_x * cos_r;
    const float l01 = -scale_x * sin_r;
    const float l10 = scale_y * sin_r;
    const float l11 = scale_y * cos_r;

    float* m = current_transform_.m;
    // 第 0 行：父变换的第 0 行与局部 2x2 线性部分组合
    m[0]  = c00 * l00 + c01 * l10;
    m[4]  = c00 * l01 + c01 * l11;
    m[12] = c00 * x + c01 * y + c03;
    // 第 1 行
    m[1]  = c10 * l00 + c11 * l10;
    m[5]  = c10 * l01 + c11 * l11;
    m[13] = c10 * x + c11 * y + c13;
    // 2D 变换不涉及 z 轴与透视，其余分量保持单位阵。
    m[2] = m[3] = m[6] = m[7] = m[8] = m[9] = m[11] = m[14] = 0.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

void opengl_renderer::pop_transform() {
    if (transform_stack_.empty()) return;
    current_transform_ = transform_stack_.back();
    transform_stack_.pop_back();
}

void opengl_renderer::push_clip(const rectangle& rect) {
    float x1 = rect.x, y1 = rect.y, x2 = rect.x + rect.width, y2 = rect.y + rect.height;
    transform_point(x1, y1);
    transform_point(x2, y2);
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    int ix = static_cast<int>(x1);
    int iy = static_cast<int>(y1);
    int iw = static_cast<int>(x2 - x1);
    int ih = static_cast<int>(y2 - y1);
    if (iw < 0) iw = 0;
    if (ih < 0) ih = 0;

    rectangle prev = clip_stack_.empty()
        ? rectangle{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)}
        : clip_stack_.back();

    int px = static_cast<int>(prev.x), py = static_cast<int>(prev.y);
    int pw = static_cast<int>(prev.width), ph = static_cast<int>(prev.height);

    int cx = std::max(ix, px);
    int cy = std::max(iy, py);
    int cw = std::min(ix + iw, px + pw) - cx;
    int ch = std::min(iy + ih, py + ph) - cy;
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    rectangle next{static_cast<float>(cx), static_cast<float>(cy),
                   static_cast<float>(cw), static_cast<float>(ch)};

    // GL 的 scissor 是逐 draw 状态，同一批次内无法切换裁剪区，所以裁剪区真正变化时
    // 必须 flush（不是旧实现那样无条件 flush）。若新裁剪区与当前有效裁剪区完全一致
    // （父控件裁剪已包含当前控件这类常见场景），则无需打断合批。
    if (next.x != prev.x || next.y != prev.y ||
        next.width != prev.width || next.height != prev.height) {
        if (!batch_.empty()) flush_batch();
        clip_stack_.push_back(next);
        apply_clip();
    } else {
        clip_stack_.push_back(next);
    }
}

void opengl_renderer::pop_clip() {
    if (clip_stack_.empty()) return;

    rectangle popped = clip_stack_.back();
    clip_stack_.pop_back();
    rectangle next = clip_stack_.empty()
        ? rectangle{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)}
        : clip_stack_.back();

    // 弹出后裁剪区没有变化时（子控件 push 的裁剪区未真正缩小）无需 flush。
    if (next.x == popped.x && next.y == popped.y &&
        next.width == popped.width && next.height == popped.height) {
        return;
    }

    if (!batch_.empty()) flush_batch();
    apply_clip();
}

void opengl_renderer::apply_clip() {
    if (clip_stack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    auto& c = clip_stack_.back();
    glEnable(GL_SCISSOR_TEST);
#if defined(__OHOS__)
    glScissor(static_cast<GLint>(c.x * density_),
              static_cast<GLint>(physical_height_ - (c.y + c.height) * density_),
              static_cast<GLsizei>(c.width * density_),
              static_cast<GLsizei>(c.height * density_));
#else
    glScissor(static_cast<GLint>(c.x), static_cast<GLint>(static_cast<float>(height_) - c.y - c.height),
              static_cast<GLsizei>(c.width), static_cast<GLsizei>(c.height));
#endif
}

void opengl_renderer::draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) {
    if (radius < 2.0f) { draw_rectangle(rect, fill_color); return; }
    float r2 = std::min(radius, std::min(rect.width, rect.height) * 0.5f);
    if (r2 < 2.0f) { draw_rectangle(rect, fill_color); return; }

    float rc = fill_color.r, gc = fill_color.g, bc = fill_color.b, ac = fill_color.a * alpha_;
    float x = rect.x, y = rect.y, w = rect.width, h = rect.height;
    // 每角 24 段的固定分段在常见的小圆角控件上会产生大量冗余顶点（单控件可达 288 个）。
    const int segs = corner_segment_count(r2);

    ensure_batch_texture(0);
    // 三条矩形（18 顶点）+ 四个角扇形（4 * segs * 3），一次性预留，避免 push 后越界。
    ensure_batch_capacity(18 + static_cast<size_t>(segs) * 12);

    draw_rect_impl(x + r2, y, w - r2 * 2.0f, h, fill_color);
    draw_rect_impl(x, y + r2, r2, h - r2 * 2.0f, fill_color);
    draw_rect_impl(x + w - r2, y + r2, r2, h - r2 * 2.0f, fill_color);

    // 角起点角度（屏幕 y 向下）：TL=π、TR=3π/2、BR=0、BL=π/2，
    // 各角按顺时针扫过四分之一圆，扇形必须落在角部方块内。
    float cxs[4] = {x + r2, x + w - r2, x + w - r2, x + r2};
    float cys[4] = {y + r2, y + r2, y + h - r2, y + h - r2};
    float sa[4] = {3.14159f, 4.71239f, 0.0f, 1.5708f};
    for (int q = 0; q < 4; ++q) {
        float cx = cxs[q], cy = cys[q];
        transform_point(cx, cy);
        for (int i = 1; i <= segs; ++i) {
            float a0 = sa[q] + 1.5708f * (i - 1) / segs;
            float a1 = sa[q] + 1.5708f * i / segs;
            push_vertex(batch_, cx, cy, 0, 0, rc, gc, bc, ac);
            push_vertex(batch_, cx + r2 * std::cos(a0), cy + r2 * std::sin(a0), 0, 0, rc, gc, bc, ac);
            push_vertex(batch_, cx + r2 * std::cos(a1), cy + r2 * std::sin(a1), 0, 0, rc, gc, bc, ac);
        }
    }
}

void opengl_renderer::draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width) {
    if (radius < 2.0f) { draw_rectangle_outline(rect, stroke_color, stroke_width); return; }
    float r2 = std::min(radius, std::min(rect.width, rect.height) * 0.5f);
    if (r2 < 2.0f) { draw_rectangle_outline(rect, stroke_color, stroke_width); return; }

    float x = rect.x, y = rect.y, w = rect.width, h = rect.height;
    float hw = stroke_width * 0.5f;
    const int segs = corner_segment_count(r2);

    ensure_batch_texture(0);
    // 四条直边（4 * 6 顶点）+ 四角粗线段（4 * segs * 6）。
    // 旧实现在循环结束后才判断上限，单次绘制最多需要 600 个顶点，可能超出 VBO 容量。
    ensure_batch_capacity(24 + static_cast<size_t>(segs) * 24);

    // 四条直边（不含角部）
    draw_rect_outline_impl(x + r2, y - hw, w - r2 * 2.0f, stroke_width, stroke_color, stroke_width);
    draw_rect_outline_impl(x + r2, y + h - hw, w - r2 * 2.0f, stroke_width, stroke_color, stroke_width);
    draw_rect_outline_impl(x - hw, y + r2, stroke_width, h - r2 * 2.0f, stroke_color, stroke_width);
    draw_rect_outline_impl(x + w - hw, y + r2, stroke_width, h - r2 * 2.0f, stroke_color, stroke_width);

    // 四角圆弧描边（沿弧线的粗线段），与直边无缝衔接
    float cxs[4] = {x + r2, x + w - r2, x + w - r2, x + r2};
    float cys[4] = {y + r2, y + r2, y + h - r2, y + h - r2};
    float sa[4] = {3.14159f, 4.71239f, 0.0f, 1.5708f};
    float r = stroke_color.r, g = stroke_color.g, b = stroke_color.b, a = stroke_color.a * alpha_;
    for (int q = 0; q < 4; ++q) {
        float cx = cxs[q], cy = cys[q];
        float prev_x = cx + r2 * std::cos(sa[q]);
        float prev_y = cy + r2 * std::sin(sa[q]);
        for (int i = 1; i <= segs; ++i) {
            float ang = sa[q] + 1.5708f * i / segs;
            float curr_x = cx + r2 * std::cos(ang);
            float curr_y = cy + r2 * std::sin(ang);

            float dx = curr_x - prev_x;
            float dy = curr_y - prev_y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f) {
                float nx = -dy / len * hw;
                float ny =  dx / len * hw;
                float ax = prev_x + nx, ay = prev_y + ny;
                float bx = prev_x - nx, by = prev_y - ny;
                float c2x = curr_x - nx, c2y = curr_y - ny;
                float d2x = curr_x + nx, d2y = curr_y + ny;
                transform_point(ax, ay);
                transform_point(bx, by);
                transform_point(c2x, c2y);
                transform_point(d2x, d2y);
                push_vertex(batch_, ax, ay, 0, 0, r, g, b, a);
                push_vertex(batch_, bx, by, 0, 0, r, g, b, a);
                push_vertex(batch_, c2x, c2y, 0, 0, r, g, b, a);
                push_vertex(batch_, ax, ay, 0, 0, r, g, b, a);
                push_vertex(batch_, c2x, c2y, 0, 0, r, g, b, a);
                push_vertex(batch_, d2x, d2y, 0, 0, r, g, b, a);
            }
            prev_x = curr_x;
            prev_y = curr_y;
        }
    }
}

void opengl_renderer::set_blend_mode(bool enabled) {
    blend_enabled_ = enabled;
    if (enabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

void opengl_renderer::set_alpha(float alpha) {
    alpha_ = std::max(0.0f, std::min(1.0f, alpha));
}

void opengl_renderer::get_viewport_size(uint32_t& width, uint32_t& height) const {
    width = width_;
    height = height_;
}

bool opengl_renderer::compile_shaders() {
    GLuint vs = compile_shader_source(GL_VERTEX_SHADER, SHAPE_VERTEX_SOURCE);
    if (!vs) return false;
    GLuint fs = compile_shader_source(GL_FRAGMENT_SHADER, SHAPE_FRAGMENT_SOURCE);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    shape_shader_.program = link_shader_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!shape_shader_.program) return false;

    shape_shader_.u_mvp = glGetUniformLocation(shape_shader_.program, "uMVP");
    shape_shader_.u_color = glGetUniformLocation(shape_shader_.program, "uColor");
    shape_shader_.u_use_texture = glGetUniformLocation(shape_shader_.program, "uUseTexture");
    shape_shader_.u_texture_sampler = glGetUniformLocation(shape_shader_.program, "uTexture");

    return true;
}

void opengl_renderer::destroy_shaders() {
    if (shape_shader_.program) {
        glDeleteProgram(shape_shader_.program);
        shape_shader_ = {};
    }
    // program 已删除，状态缓存必须失效，否则后续 draw 会误以为 program/mvp 仍有效。
    cached_program_ = 0;
    mvp_uploaded_ = false;
}

bool opengl_renderer::create_buffers() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const int vbo_capacity = MAX_BATCH_VERTICES + 512;
    glBufferData(GL_ARRAY_BUFFER, vbo_capacity * sizeof(vertex), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex),
                          reinterpret_cast<const void*>(offsetof(vertex, x)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex),
                          reinterpret_cast<const void*>(offsetof(vertex, u)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex),
                          reinterpret_cast<const void*>(offsetof(vertex, r)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void opengl_renderer::destroy_buffers() {
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

void opengl_renderer::flush_batch() {
    if (batch_.empty()) return;

    // program 与 MVP 在一帧内不变：缓存后避免每个批次都重复 glUseProgram /
    // glUniformMatrix4fv（控件繁多时每帧可能提交数十个批次）。
    if (cached_program_ != shape_shader_.program) {
        glUseProgram(shape_shader_.program);
        cached_program_ = shape_shader_.program;
        mvp_uploaded_ = false;
    }
    if (!mvp_uploaded_) {
        glUniformMatrix4fv(shape_shader_.u_mvp, 1, GL_FALSE, mvp_matrix_);
        mvp_uploaded_ = true;
    }

    if (batch_has_texture_) {
        glUniform1i(shape_shader_.u_use_texture, 1);
        glUniform1i(shape_shader_.u_texture_sampler, 0);
        glActiveTexture(GL_TEXTURE0);
        // 批次按纹理分批，这里直接绑定批次记录的纹理（字形图集或图片）。
        glBindTexture(GL_TEXTURE_2D, current_texture_);
    } else {
        glUniform1i(shape_shader_.u_use_texture, 0);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(batch_.size() * sizeof(vertex)),
                    batch_.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch_.size()));

    batch_.clear();
    batch_has_texture_ = false;
    current_texture_ = 0;
}


opengl_renderer::image_resource* opengl_renderer::load_image(const std::string& path) {
    auto it = images_.find(path);
    if (it != images_.end()) return &it->second;

    int w, h, channels;
    // 纹理坐标约定：本渲染器的四边形把 v=0 放在矩形顶部（见 draw_image），
    // 而 glTexImage2D 的第一行数据对应 v=0。stb 默认按文件顺序输出（第 0 行即图像顶部），
    // 因此这里不能翻转；旧实现设置为 1，会使所有图片上下颠倒（与 Direct2D/Metal 后端不一致）。
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("failed to load image: {}", path), "renderer/opengl"});
        return nullptr;
    }

    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    image_resource resource{tex_id, static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    auto result = images_.emplace(path, resource);
    return &result.first->second;
}

void opengl_renderer::transform_point(float& x, float& y) const {
    const float* m = current_transform_.m;
    float tx = m[0] * x + m[4] * y + m[12];
    float ty = m[1] * x + m[5] * y + m[13];
    x = tx;
    y = ty;
}

void opengl_renderer::draw_rect_impl(float x, float y, float w, float h, const color& fill_color) {
    ensure_batch_texture(0);
    ensure_batch_capacity(6);

    float x1 = x, y1 = y;
    float x2 = x + w, y2 = y + h;
    transform_point(x1, y1);
    transform_point(x2, y2);
    w = x2 - x1;
    h = y2 - y1;

    float r = fill_color.r, g = fill_color.g, b = fill_color.b, a = fill_color.a * alpha_;
    push_rect_vertices(batch_, x1, y1, w, h, 0, 0, 0, 0, r, g, b, a);
}

void opengl_renderer::draw_rect_outline_impl(float x, float y, float w, float h,
                                              const color& stroke_color, float stroke_width) {
    ensure_batch_texture(0);
    ensure_batch_capacity(24);

    float x1 = x, y1 = y;
    float x2 = x + w, y2 = y + h;
    transform_point(x1, y1);
    transform_point(x2, y2);
    w = x2 - x1;
    h = y2 - y1;

    float r = stroke_color.r, g = stroke_color.g, b = stroke_color.b, a = stroke_color.a * alpha_;
    float hw = stroke_width * 0.5f;

    push_rect_vertices(batch_, x1 - hw, y1 - hw, w + stroke_width, stroke_width, 0, 0, 0, 0, r, g, b, a);
    push_rect_vertices(batch_, x1 - hw, y1 + h, w + stroke_width, stroke_width, 0, 0, 0, 0, r, g, b, a);
    push_rect_vertices(batch_, x1 - hw, y1, stroke_width, h, 0, 0, 0, 0, r, g, b, a);
    push_rect_vertices(batch_, x1 + w - hw, y1, stroke_width, h, 0, 0, 0, 0, r, g, b, a);
}

void opengl_renderer::draw_circle_impl(float cx, float cy, float radius, const color& fill_color, bool filled, float stroke_width) {
    transform_point(cx, cy);
    float r = fill_color.r, g = fill_color.g, b = fill_color.b, a = fill_color.a * alpha_;
    const int segments = circle_segment_count(radius);

    ensure_batch_texture(0);
    // 填充圆：segments 个扇形，每个 3 顶点；描边圆：每段 2 个三角形，每个 6 顶点。
    ensure_batch_capacity(static_cast<size_t>(segments) * (filled ? 3 : 6));

    if (filled) {
        float prev_x = cx + radius;
        float prev_y = cy;
        for (int i = 1; i <= segments; ++i) {
            float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(segments);
            float curr_x = cx + radius * std::cos(angle);
            float curr_y = cy + radius * std::sin(angle);

            push_vertex(batch_, cx, cy, 0, 0, r, g, b, a);
            push_vertex(batch_, prev_x, prev_y, 0, 0, r, g, b, a);
            push_vertex(batch_, curr_x, curr_y, 0, 0, r, g, b, a);

            prev_x = curr_x;
            prev_y = curr_y;
        }
    } else {
        float prev_x = cx + radius;
        float prev_y = cy;
        // 旧实现把描边宽度硬编码为 1.0f，draw_circle_outline 传入的 stroke_width 被丢弃。
        const float hw = stroke_width * 0.5f;

        for (int i = 1; i <= segments; ++i) {
            float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(segments);
            float curr_x = cx + radius * std::cos(angle);
            float curr_y = cy + radius * std::sin(angle);

            float dx = curr_x - prev_x;
            float dy = curr_y - prev_y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f) {
                float nx = -dy / len * hw;
                float ny =  dx / len * hw;

                float ax = prev_x + nx, ay = prev_y + ny;
                float bx = prev_x - nx, by = prev_y - ny;
                float cx2 = curr_x - nx, cy2 = curr_y - ny;
                float dx2 = curr_x + nx, dy2 = curr_y + ny;

                push_vertex(batch_, ax, ay, 0, 0, r, g, b, a);
                push_vertex(batch_, bx, by, 0, 0, r, g, b, a);
                push_vertex(batch_, cx2, cy2, 0, 0, r, g, b, a);
                push_vertex(batch_, ax, ay, 0, 0, r, g, b, a);
                push_vertex(batch_, cx2, cy2, 0, 0, r, g, b, a);
                push_vertex(batch_, dx2, dy2, 0, 0, r, g, b, a);
            }

            prev_x = curr_x;
            prev_y = curr_y;
        }
    }
}

#if defined(__APPLE__)
bool opengl_renderer::init_gl_context(void* native_window) {
    // native_window 为窗口持有的 NSOpenGLContext 句柄（见 cocoa_window）。
    gl_context_ = native_window;
    if (!gl_context_) return false;
    if (!spiration_gl_make_current(gl_context_)) {
        gl_context_ = nullptr;
        return false;
    }
    return true;
}

void opengl_renderer::destroy_gl_context() {
    // 上下文由窗口创建并销毁，渲染器仅解除引用。
    gl_context_ = nullptr;
}
#endif

std::shared_ptr<renderer> renderer::create_opengl_renderer() {
    return std::make_shared<opengl_renderer>();
}

} // namespace spiration
#endif
