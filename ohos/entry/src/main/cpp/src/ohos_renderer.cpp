/**
 * @file ohos_renderer.cpp
 * @brief OHOS OpenGL ES 3.0 渲染器实现 — 批处理渲染 + 统一着色器。
 * @author clk
 */

#include "ohos_renderer.h"
#include <utils/console.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <native_drawing/drawing_bitmap.h>
#include <native_drawing/drawing_canvas.h>
#include <native_drawing/drawing_text_typography.h>
#include <native_drawing/drawing_font_collection.h>
#include <native_drawing/drawing_color.h>
#include <native_drawing/drawing_types.h>
#include <native_drawing/drawing_rect.h>

namespace spiration {

static const char* UNIFIED_VS = R"(#version 300 es
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
)";

static const char* UNIFIED_FS = R"(#version 300 es
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
)";

void ohos_renderer::mat4_identity(float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void ohos_renderer::mat4_multiply(float* r, const float* a, const float* b) {
    float tmp[16];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            tmp[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] +
                             a[i * 4 + 1] * b[1 * 4 + j] +
                             a[i * 4 + 2] * b[2 * 4 + j] +
                             a[i * 4 + 3] * b[3 * 4 + j];
    std::memcpy(r, tmp, sizeof(tmp));
}

void ohos_renderer::mat4_ortho(float* m, float l, float r, float b, float t) {
    mat4_identity(m);
    m[0]  = 2.0f / (r - l);
    m[5]  = 2.0f / (t - b);
    m[10] = -1.0f;
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
}

void ohos_renderer::mat4_translate(float* m, float x, float y, float z) {
    float t[16]; mat4_identity(t); t[12] = x; t[13] = y; t[14] = z;
    float r[16]; mat4_multiply(r, m, t); std::memcpy(m, r, sizeof(r));
}

void ohos_renderer::mat4_rotate_z(float* m, float deg) {
    float rad = deg * 3.14159265f / 180.0f, c = std::cos(rad), s = std::sin(rad);
    float r[16]; mat4_identity(r); r[0] = c; r[1] = s; r[4] = -s; r[5] = c;
    float res[16]; mat4_multiply(res, m, r); std::memcpy(m, res, sizeof(res));
}

void ohos_renderer::mat4_scale(float* m, float sx, float sy, float sz) {
    float s[16]; mat4_identity(s); s[0] = sx; s[5] = sy; s[10] = sz;
    float r[16]; mat4_multiply(r, m, s); std::memcpy(m, r, sizeof(r));
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    if (!s) return 0;
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        console::error("ohos_renderer", "shader compile error (%d): %s", type, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    if (!p) return 0;
    glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        console::error("ohos_renderer", "link error: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

ohos_renderer::ohos_renderer() {
    mat4_identity(current_transform_.m);
    mat4_identity(proj_);
    mat4_identity(mvp_);
}

ohos_renderer::~ohos_renderer() { shutdown(); }

bool ohos_renderer::initialize(void* native_window_handle) {
    if (!init_egl(native_window_handle)) {
        console::error("ohos_renderer", "EGL init failed");
        return false;
    }
    if (!init_shaders()) {
        console::error("ohos_renderer", "shader init failed");
        return false;
    }
    if (!create_buffers()) {
        console::error("ohos_renderer", "buffer init failed");
        return false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    console::info("ohos_renderer", "initialized (%dx%d)", viewport_width_, viewport_height_);
    return true;
}

void ohos_renderer::shutdown() {
    flush_batch();

    for (auto& [path, img] : images_) {
        if (img.texture_id) glDeleteTextures(1, &img.texture_id);
    }
    images_.clear();

    destroy_buffers();

    if (shader_program_) { glDeleteProgram(shader_program_); shader_program_ = 0; }

    if (egl_display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context_ != EGL_NO_CONTEXT) eglDestroyContext(egl_display_, egl_context_);
        if (egl_surface_ != EGL_NO_SURFACE) eglDestroySurface(egl_display_, egl_surface_);
        eglTerminate(egl_display_);
    }
    egl_display_ = EGL_NO_DISPLAY;
    egl_context_ = EGL_NO_CONTEXT;
    egl_surface_ = EGL_NO_SURFACE;
}

void ohos_renderer::resize(uint32_t width, uint32_t height) {
    if (viewport_width_ == width && viewport_height_ == height) return;
    viewport_width_ = width;
    viewport_height_ = height;
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    setup_projection();
}

void ohos_renderer::set_logical_size(uint32_t width, uint32_t height) {
    viewport_width_ = width;
    viewport_height_ = height;
    setup_projection();
}

void ohos_renderer::check_resize() {
    if (egl_display_ == EGL_NO_DISPLAY || egl_surface_ == EGL_NO_SURFACE) return;
    EGLint px_w = 0, px_h = 0;
    eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH, &px_w);
    eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &px_h);
    uint32_t uw = static_cast<uint32_t>(px_w);
    uint32_t uh = static_cast<uint32_t>(px_h);
    if (uw == last_egl_w_ && uh == last_egl_h_) return;
    last_egl_w_ = uw;
    last_egl_h_ = uh;
    if (uw == 0 || uh == 0) return;

    physical_width_ = uw;
    physical_height_ = uh;

    uint32_t vp_w = static_cast<uint32_t>(uw / density_);
    uint32_t vp_h = static_cast<uint32_t>(uh / density_);

    glViewport(0, 0, static_cast<GLsizei>(uw), static_cast<GLsizei>(uh));
    viewport_width_ = vp_w;
    viewport_height_ = vp_h;
    setup_projection();
}

bool ohos_renderer::init_egl(void* native_window) {
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };

    egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display_ == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(egl_display_, nullptr, nullptr)) return false;

    EGLint nc = 0;
    if (!eglChooseConfig(egl_display_, attribs, &egl_config_, 1, &nc) || nc == 0) return false;

    egl_surface_ = eglCreateWindowSurface(egl_display_, egl_config_,
                                          reinterpret_cast<EGLNativeWindowType>(native_window), nullptr);
    if (egl_surface_ == EGL_NO_SURFACE) return false;

    egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, ctx_attr);
    if (egl_context_ == EGL_NO_CONTEXT) return false;
    if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) return false;

    EGLint w = 0, h = 0;
    eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH, &w);
    eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &h);
    resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    return true;
}

bool ohos_renderer::init_shaders() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, UNIFIED_VS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, UNIFIED_FS);
    if (!vs || !fs) return false;

    shader_program_ = link_program(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!shader_program_) return false;

    u_mvp_loc_ = glGetUniformLocation(shader_program_, "uMVP");
    u_color_loc_ = glGetUniformLocation(shader_program_, "uColor");
    u_use_tex_loc_ = glGetUniformLocation(shader_program_, "uUseTexture");
    u_tex_loc_ = glGetUniformLocation(shader_program_, "uTexture");
    return true;
}

bool ohos_renderer::create_buffers() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, MAX_BATCH_VERTS * sizeof(vertex), nullptr, GL_DYNAMIC_DRAW);

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

void ohos_renderer::destroy_buffers() {
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
}

void ohos_renderer::setup_projection() {
    mat4_ortho(proj_, 0.0f, static_cast<float>(viewport_width_),
               static_cast<float>(viewport_height_), 0.0f);
}

void ohos_renderer::apply_transform() {
    mat4_multiply(mvp_, proj_, current_transform_.m);
}

void ohos_renderer::flush_batch() {
    if (batch_.empty()) return;

    glUseProgram(shader_program_);
    glUniformMatrix4fv(u_mvp_loc_, 1, GL_FALSE, mvp_);

    if (batch_has_texture_) {
        glUniform1i(u_use_tex_loc_, 1);
        glUniform1i(u_tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0); // caller must bind before adding to batch
    } else {
        glUniform1i(u_use_tex_loc_, 0);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(batch_.size() * sizeof(vertex)),
                    batch_.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch_.size()));

    batch_.clear();
    batch_has_texture_ = false;
}

void ohos_renderer::transform_point(float& x, float& y) const {
    const float* m = current_transform_.m;
    float tx = m[0] * x + m[4] * y + m[12];
    float ty = m[1] * x + m[5] * y + m[13];
    x = tx; y = ty;
}

void ohos_renderer::push_rect_verts(float x, float y, float w, float h,
                                    float r, float g, float b, float a) {
    if (batch_has_texture_) flush_batch();
    vertex verts[6] = {
        {x, y, 0,0, r,g,b,a}, {x+w,y, 0,0, r,g,b,a}, {x,y+h, 0,0, r,g,b,a},
        {x+w,y, 0,0, r,g,b,a}, {x+w,y+h, 0,0, r,g,b,a}, {x,y+h, 0,0, r,g,b,a},
    };
    for (auto& v : verts) batch_.push_back(v);
    if (batch_.size() >= MAX_BATCH_VERTS) flush_batch();
}

void ohos_renderer::push_tex_verts(float x, float y, float w, float h,
                                   float u0, float v0, float u1, float v1,
                                   float r, float g, float b, float a) {
    vertex verts[6] = {
        {x, y, u0,v0, r,g,b,a}, {x+w,y, u1,v0, r,g,b,a}, {x,y+h, u0,v1, r,g,b,a},
        {x+w,y, u1,v0, r,g,b,a}, {x+w,y+h, u1,v1, r,g,b,a}, {x,y+h, u0,v1, r,g,b,a},
    };
    for (auto& v : verts) batch_.push_back(v);
    batch_has_texture_ = true;
    if (batch_.size() >= MAX_BATCH_VERTS) flush_batch();
}

void ohos_renderer::begin_frame() {
    batch_.clear();
    batch_.reserve(1024);
    batch_has_texture_ = false;

    transform_stack_.clear();
    mat4_identity(current_transform_.m);
    apply_transform();

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (blend_enabled_) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void ohos_renderer::end_frame() {
    flush_batch();
    eglSwapBuffers(egl_display_, egl_surface_);
}

void ohos_renderer::clear(const color& c) {
    flush_batch();
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ohos_renderer::draw_rectangle(const rectangle& rect, const color& fill_color) {
    float x1 = rect.x, y1 = rect.y;
    float x2 = rect.x + rect.width, y2 = rect.y + rect.height;
    transform_point(x1, y1);
    transform_point(x2, y2);
    float w = x2 - x1, h = y2 - y1;
    float a = fill_color.a * current_alpha_;
    push_rect_verts(x1, y1, w, h, fill_color.r, fill_color.g, fill_color.b, a);
}

void ohos_renderer::draw_rectangle_outline(const rectangle& rect, const color& stroke_color,
                                           float stroke_width) {
    if (batch_has_texture_) flush_batch();
    float x1 = rect.x, y1 = rect.y;
    float x2 = rect.x + rect.width, y2 = rect.y + rect.height;
    transform_point(x1, y1);
    transform_point(x2, y2);
    float w = x2 - x1, h = y2 - y1;
    float r = stroke_color.r, g = stroke_color.g, ba = stroke_color.b, a = stroke_color.a * current_alpha_;
    float hw = stroke_width * 0.5f;
    push_rect_verts(x1 - hw, y1 - hw, w + stroke_width, stroke_width, r, g, ba, a);
    push_rect_verts(x1 - hw, y1 + h, w + stroke_width, stroke_width, r, g, ba, a);
    push_rect_verts(x1 - hw, y1, stroke_width, h, r, g, ba, a);
    push_rect_verts(x1 + w - hw, y1, stroke_width, h, r, g, ba, a);
}

void ohos_renderer::draw_circle(const point& center, float radius, const color& fill_color) {
    if (batch_has_texture_) flush_batch();
    float cx = center.x, cy = center.y;
    transform_point(cx, cy);
    constexpr int SEGS = 32;
    float r = fill_color.r, g = fill_color.g, b = fill_color.b, a = fill_color.a * current_alpha_;
    float px = cx + radius, py = cy;
    for (int i = 1; i <= SEGS; ++i) {
        float angle = 6.283185f * i / SEGS;
        float nx = cx + radius * std::cos(angle);
        float ny = cy + radius * std::sin(angle);
        batch_.push_back({cx,cy, 0,0, r,g,b,a});
        batch_.push_back({px,py, 0,0, r,g,b,a});
        batch_.push_back({nx,ny, 0,0, r,g,b,a});
        px = nx; py = ny;
    }
    if (batch_.size() >= MAX_BATCH_VERTS) flush_batch();
}

void ohos_renderer::draw_circle_outline(const point& center, float radius, const color& stroke_color,
                                        float stroke_width) {
    flush_batch();
    float cx = center.x, cy = center.y;
    transform_point(cx, cy);
    constexpr int SEGS = 32;
    float r = stroke_color.r, g = stroke_color.g, b = stroke_color.b, a = stroke_color.a * current_alpha_;
    float px = cx + radius, py = cy;
    for (int i = 1; i <= SEGS; ++i) {
        float angle = 6.283185f * i / SEGS;
        float nx = cx + radius * std::cos(angle);
        float ny = cy + radius * std::sin(angle);
        float dx = nx - px, dy = ny - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.001f) {
            float hwx = -dy / len * stroke_width * 0.5f;
            float hwy = dx / len * stroke_width * 0.5f;
            batch_.push_back({px + hwx, py + hwy, 0,0, r,g,b,a});
            batch_.push_back({px - hwx, py - hwy, 0,0, r,g,b,a});
            batch_.push_back({nx - hwy, ny + hwx, 0,0, r,g,b,a}); // approx
            batch_.push_back({px + hwx, py + hwy, 0,0, r,g,b,a});
            batch_.push_back({nx - hwy, ny + hwx, 0,0, r,g,b,a});
            batch_.push_back({nx + hwy, ny - hwx, 0,0, r,g,b,a});
        }
        px = nx; py = ny;
    }
    if (batch_.size() >= MAX_BATCH_VERTS) flush_batch();
}

void ohos_renderer::draw_line(const point& start, const point& end, const color& stroke_color,
                              float stroke_width) {
    if (batch_has_texture_) flush_batch();
    float sx = start.x, sy = start.y, ex = end.x, ey = end.y;
    transform_point(sx, sy); transform_point(ex, ey);
    float dx = ex - sx, dy = ey - sy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * stroke_width * 0.5f;
    float ny = dx / len * stroke_width * 0.5f;
    float r = stroke_color.r, g = stroke_color.g, ba = stroke_color.b, a = stroke_color.a * current_alpha_;
    batch_.push_back({sx + nx, sy + ny, 0,0, r,g,ba,a});
    batch_.push_back({sx - nx, sy - ny, 0,0, r,g,ba,a});
    batch_.push_back({ex - nx, ey - ny, 0,0, r,g,ba,a});
    batch_.push_back({sx + nx, sy + ny, 0,0, r,g,ba,a});
    batch_.push_back({ex - nx, ey - ny, 0,0, r,g,ba,a});
    batch_.push_back({ex + nx, ey + ny, 0,0, r,g,ba,a});
    if (batch_.size() >= MAX_BATCH_VERTS) flush_batch();
}

void ohos_renderer::draw_text(const std::string& text, const point& position,
                              const color& text_color, float font_size,
                              const std::string& font_family,
                              bool word_wrap) {
    if (text.empty()) return;

    // 调试：打印文本渲染参数（已禁用）
    // if (text.size() < 50) {
    //     console::info("text", "draw \"%s\" at (%.1f,%.1f) size=%.1f",
    //                   text.c_str(), position.x, position.y, font_size);
    // }

    float dpi = density_ > 0.0f ? density_ : 1.0f;

    OH_Drawing_TypographyStyle* style = OH_Drawing_CreateTypographyStyle();
    OH_Drawing_SetTypographyTextDirection(style, TEXT_DIRECTION_LTR);
    OH_Drawing_SetTypographyTextAlign(style, TEXT_ALIGN_LEFT);
    OH_Drawing_FontCollection* fc = OH_Drawing_CreateFontCollection();
    OH_Drawing_TypographyCreate* handler = OH_Drawing_CreateTypographyHandler(style, fc);
    OH_Drawing_TextStyle* tStyle = OH_Drawing_CreateTextStyle();
    OH_Drawing_SetTextStyleColor(tStyle, OH_Drawing_ColorSetArgb(255, 255, 255, 255));
    OH_Drawing_SetTextStyleFontSize(tStyle, static_cast<double>(font_size * dpi));
    OH_Drawing_TypographyHandlerPushTextStyle(handler, tStyle);
    OH_Drawing_TypographyHandlerAddText(handler, text.c_str());
    OH_Drawing_Typography* typo = OH_Drawing_CreateTypography(handler);
    if (word_wrap) {
        double wrap_width = static_cast<double>(viewport_width_ * dpi);
        if (position.x < static_cast<float>(viewport_width_)) {
            wrap_width = static_cast<double>((viewport_width_ - position.x) * dpi);
        }
        OH_Drawing_TypographyLayout(typo, wrap_width);
    } else {
        OH_Drawing_TypographyLayout(typo, 2000.0 * dpi);
    }

    float tw_logical = static_cast<float>(OH_Drawing_TypographyGetLongestLine(typo)) / dpi + 4.0f;
    float th_logical = static_cast<float>(OH_Drawing_TypographyGetHeight(typo)) / dpi + 4.0f;
    int bw = std::max(1, static_cast<int>(std::ceil(tw_logical * dpi)));
    int bh = std::max(1, static_cast<int>(std::ceil(th_logical * dpi)));

    OH_Drawing_Bitmap* bmp = OH_Drawing_BitmapCreate();
    OH_Drawing_BitmapFormat fmt = { COLOR_FORMAT_RGBA_8888, ALPHA_FORMAT_UNPREMUL };
    OH_Drawing_BitmapBuild(bmp, bw, bh, &fmt);
    OH_Drawing_Canvas* canvas = OH_Drawing_CanvasCreate();
    OH_Drawing_CanvasBind(canvas, bmp);
    OH_Drawing_CanvasClear(canvas, OH_Drawing_ColorSetArgb(0, 0, 0, 0));
    float pad = 2.0f * dpi;
    OH_Drawing_TypographyPaint(typo, canvas, pad, pad);

    void* pixels = OH_Drawing_BitmapGetPixels(bmp);
    if (pixels) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        flush_batch();
        float x = position.x, y = position.y;
        transform_point(x, y);

        glUseProgram(shader_program_);
        glUniformMatrix4fv(u_mvp_loc_, 1, GL_FALSE, proj_);
        glUniform1i(u_use_tex_loc_, 1);
        glUniform1i(u_tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        float a = text_color.a * current_alpha_;
        float tw = tw_logical;
        float th = th_logical;
        vertex verts[6] = {
            {x, y, 0,0, text_color.r,text_color.g,text_color.b,a},
            {x+tw, y, 1,0, text_color.r,text_color.g,text_color.b,a},
            {x, y+th, 0,1, text_color.r,text_color.g,text_color.b,a},
            {x+tw, y, 1,0, text_color.r,text_color.g,text_color.b,a},
            {x+tw, y+th, 1,1, text_color.r,text_color.g,text_color.b,a},
            {x, y+th, 0,1, text_color.r,text_color.g,text_color.b,a},
        };
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glUniform1i(u_use_tex_loc_, 0);

        glDeleteTextures(1, &tex);
    }

    OH_Drawing_DestroyTypography(typo);
    OH_Drawing_CanvasDestroy(canvas);
    OH_Drawing_BitmapDestroy(bmp);
    OH_Drawing_DestroyTypographyHandler(handler);
    OH_Drawing_DestroyTextStyle(tStyle);
    OH_Drawing_DestroyFontCollection(fc);
    OH_Drawing_DestroyTypographyStyle(style);
}

void ohos_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds,
                                      const color& text_color,
                                      text_alignment h_align, vertical_alignment v_align,
                                      float font_size, const std::string& font_family) {
    if (text.empty()) return;
    float tw = measure_text_width(text, font_size, font_family);
    float th = font_size;

    float x = bounds.x;
    switch (h_align) {
        case text_alignment::left:   x = bounds.x; break;
        case text_alignment::center: x = bounds.x + (bounds.width - tw) * 0.5f; break;
        case text_alignment::right:  x = bounds.x + bounds.width - tw; break;
    }
    float y = bounds.y;
    switch (v_align) {
        case vertical_alignment::top:    y = bounds.y; break;
        case vertical_alignment::center: y = bounds.y + (bounds.height - th) * 0.5f; break;
        case vertical_alignment::bottom: y = bounds.y + bounds.height - th; break;
    }
    draw_text(text, {x, y}, text_color, font_size, font_family);
}

ohos_renderer::image_resource* ohos_renderer::load_image(const std::string& path) {
    auto it = images_.find(path);
    if (it != images_.end()) return &it->second;

    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        console::warning("ohos_renderer", "failed to load image: %s", path.c_str());
        return nullptr;
    }

    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);

    image_resource resource{tex_id, static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    auto result = images_.emplace(path, resource);
    return &result.first->second;
}

void ohos_renderer::draw_image(const std::string& image_path, const rectangle& destination) {
    image_resource* img = load_image(image_path);
    if (!img) {
        draw_rectangle(destination, {0.3f, 0.3f, 0.3f, 1.0f});
        return;
    }

    float x1 = destination.x, y1 = destination.y;
    float x2 = destination.x + destination.width, y2 = destination.y + destination.height;
    transform_point(x1, y1);
    transform_point(x2, y2);
    float w = x2 - x1, h = y2 - y1;

    flush_batch();
    glBindTexture(GL_TEXTURE_2D, img->texture_id);
    push_tex_verts(x1, y1, w, h, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, current_alpha_);
}

void ohos_renderer::draw_image_subregion(const std::string& image_path, const rectangle& source,
                                         const rectangle& destination) {
    image_resource* img = load_image(image_path);
    if (!img) {
        draw_rectangle(destination, {0.3f, 0.3f, 0.3f, 1.0f});
        return;
    }

    float sx = source.x / static_cast<float>(img->width);
    float sy = source.y / static_cast<float>(img->height);
    float sw = source.width / static_cast<float>(img->width);
    float sh = source.height / static_cast<float>(img->height);

    float x1 = destination.x, y1 = destination.y;
    float x2 = destination.x + destination.width, y2 = destination.y + destination.height;
    transform_point(x1, y1);
    transform_point(x2, y2);
    float w = x2 - x1, h = y2 - y1;

    flush_batch();
    glBindTexture(GL_TEXTURE_2D, img->texture_id);
    push_tex_verts(x1, y1, w, h, sx, sy, sx + sw, sy + sh, 1.0f, 1.0f, 1.0f, current_alpha_);
}

void ohos_renderer::push_transform(float x, float y, float rotation,
                                   float scale_x, float scale_y) {
    transform_stack_.push_back(current_transform_);
    float local[16];
    mat4_identity(local);
    float rad = rotation * 3.14159265f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    local[0] = scale_x * c;  local[1] = scale_y * s;
    local[4] = -scale_x * s; local[5] = scale_y * c;
    local[12] = x;           local[13] = y;
    float result[16];
    mat4_multiply(result, local, current_transform_.m);
    std::memcpy(current_transform_.m, result, sizeof(result));
}

void ohos_renderer::pop_transform() {
    if (transform_stack_.empty()) return;
    current_transform_ = transform_stack_.back();
    transform_stack_.pop_back();
}

void ohos_renderer::push_clip(const rectangle& rect) {
    if (!batch_.empty()) flush_batch();

    // 变换到屏幕坐标（UI 坐标系）
    float x1 = rect.x, y1 = rect.y, x2 = rect.x + rect.width, y2 = rect.y + rect.height;
    transform_point(x1, y1);
    transform_point(x2, y2);
    
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    int ix = static_cast<int>(x1), iy = static_cast<int>(y1);
    int iw = static_cast<int>(x2 - x1), ih = static_cast<int>(y2 - y1);
    if (iw < 0) iw = 0;
    if (ih < 0) ih = 0;

    // 获取前一个 clip 区域（UI 坐标系）
    rectangle prev = clip_stack_.empty()
        ? rectangle{0.0f, 0.0f, static_cast<float>(viewport_width_), static_cast<float>(viewport_height_)}
        : clip_stack_.back();
    int px = static_cast<int>(prev.x), py = static_cast<int>(prev.y);
    int pw = static_cast<int>(prev.width), ph = static_cast<int>(prev.height);

    // 计算交集
    int cx = std::max(ix, px), cy = std::max(iy, py);
    int cw = std::min(ix + iw, px + pw) - cx;
    int ch = std::min(iy + ih, py + ph) - cy;
    if (cw < 0) cw = 0; if (ch < 0) ch = 0;

    // 存储 UI 坐标系，glScissor 时转换
    clip_stack_.push_back({static_cast<float>(cx), static_cast<float>(cy),
                           static_cast<float>(cw), static_cast<float>(ch)});
    if (clip_stack_.size() == 1) glEnable(GL_SCISSOR_TEST);
    auto& c = clip_stack_.back();

    // UI 坐标系 → OpenGL 坐标系（像素坐标）
    // glScissor 使用窗口坐标（像素），需要乘以 density_
    float density = density_ > 0.0f ? density_ : 1.0f;
    GLint gl_x = static_cast<GLint>(c.x * density);
    // 使用物理像素高度进行 Y 轴翻转
    GLint gl_y = static_cast<GLint>(physical_height_ - (c.y + c.height) * density_);
    GLsizei gl_w = static_cast<GLsizei>(c.width * density);
    GLsizei gl_h = static_cast<GLsizei>(c.height * density);

    glScissor(gl_x, gl_y, gl_w, gl_h);
}

void ohos_renderer::pop_clip() {
    if (!batch_.empty()) flush_batch();
    if (clip_stack_.empty()) return;
    clip_stack_.pop_back();
    if (clip_stack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        auto& c = clip_stack_.back();
        // UI 坐标系 → OpenGL 坐标系（像素坐标）
        float density = density_ > 0.0f ? density_ : 1.0f;
        // 使用物理像素高度进行 Y 轴翻转
        glScissor(static_cast<GLint>(c.x * density),
                  static_cast<GLint>(physical_height_ - (c.y + c.height) * density),
                  static_cast<GLsizei>(c.width * density),
                  static_cast<GLsizei>(c.height * density));
    }
}

void ohos_renderer::draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) {
    if (radius < 2.0f) { draw_rectangle(rect, fill_color); return; }
    float r2 = std::min(radius, std::min(rect.width, rect.height) * 0.5f);
    if (r2 < 2.0f) { draw_rectangle(rect, fill_color); return; }

    float x = rect.x, y = rect.y, w = rect.width, h = rect.height;
    float rc = fill_color.r, gc = fill_color.g, bc = fill_color.b, ac = fill_color.a * current_alpha_;
    if (batch_has_texture_) flush_batch();

    // 应用变换到矩形的四个角点
    float x1 = x + r2, y1 = y;
    float x2 = x + w - r2, y2 = y + h;
    transform_point(x1, y1);
    transform_point(x2, y2);
    float tw = x2 - x1, th = y2 - y1;
    float tr2 = r2; // 圆角半径在变换后保持不变（假设无缩放）

    push_rect_verts(x1 + tr2, y1, tw - tr2 * 2.0f, th, rc, gc, bc, ac);
    push_rect_verts(x1, y1 + tr2, tr2, th - tr2 * 2.0f, rc, gc, bc, ac);
    push_rect_verts(x2 - tr2, y1 + tr2, tr2, th - tr2 * 2.0f, rc, gc, bc, ac);

    constexpr int SEGS = 8;
    float cxs[4] = {x + r2, x + w - r2, x + w - r2, x + r2};
    float cys[4] = {y + r2, y + r2, y + h - r2, y + h - r2};
    float sa[4] = {3.14159f, 0.0f, 1.5708f, 4.7124f};
    for (int q = 0; q < 4; ++q) {
        float cx = cxs[q], cy = cys[q];
        transform_point(cx, cy);
        for (int i = 1; i <= SEGS; ++i) {
            float a0 = sa[q] + 1.5708f * (i - 1) / SEGS;
            float a1 = sa[q] + 1.5708f * i / SEGS;
            batch_.push_back({cx, cy, 0, 0, rc, gc, bc, ac});
            batch_.push_back({cx + tr2 * std::cos(a0), cy + tr2 * std::sin(a0), 0, 0, rc, gc, bc, ac});
            batch_.push_back({cx + tr2 * std::cos(a1), cy + tr2 * std::sin(a1), 0, 0, rc, gc, bc, ac});
        }
    }
    if (batch_.size() >= MAX_BATCH_VERTS) flush_batch();
}

void ohos_renderer::draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width) {
    if (radius < 2.0f) { draw_rectangle_outline(rect, stroke_color, stroke_width); return; }
    float r2 = std::min(radius, std::min(rect.width, rect.height) * 0.5f);
    if (r2 < 2.0f) { draw_rectangle_outline(rect, stroke_color, stroke_width); return; }

    float x = rect.x, y = rect.y, w = rect.width, h = rect.height;
    float hw = stroke_width * 0.5f;
    float rc = stroke_color.r, gc = stroke_color.g, bc = stroke_color.b, ac = stroke_color.a * current_alpha_;

    // 应用变换到矩形的四个角点
    float x1 = x, y1 = y;
    float x2 = x + w, y2 = y + h;
    transform_point(x1, y1);
    transform_point(x2, y2);
    float tw = x2 - x1, th = y2 - y1;

    push_rect_verts(x1 + r2, y1 - hw, tw - r2 * 2.0f, stroke_width, rc, gc, bc, ac);
    push_rect_verts(x1 + r2, y2 - hw, tw - r2 * 2.0f, stroke_width, rc, gc, bc, ac);
    push_rect_verts(x1 - hw, y1 + r2, stroke_width, th - r2 * 2.0f, rc, gc, bc, ac);
    push_rect_verts(x2 - hw, y1 + r2, stroke_width, th - r2 * 2.0f, rc, gc, bc, ac);
}

void ohos_renderer::set_blend_mode(bool enabled) {
    blend_enabled_ = enabled;
    if (enabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}

void ohos_renderer::set_alpha(float alpha) {
    current_alpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void ohos_renderer::get_viewport_size(uint32_t& width, uint32_t& height) const {
    width = viewport_width_;
    height = viewport_height_;
}

float ohos_renderer::measure_text_width(const std::string& text, float font_size,
                                        const std::string& font_family) {
    if (text.empty()) return 0.0f;
    return text.length() * font_size * 0.5f;
}

float ohos_renderer::measure_text_height(const std::string& text, float font_size,
                                          const std::string& font_family,
                                          float wrap_width) {
    // 简单实现：假设单行文本，高度等于字体大小
    // TODO: 实现多行文本高度计算（考虑 wrap_width）
    if (text.empty()) return 0.0f;
    return font_size;
}

} // namespace spiration
