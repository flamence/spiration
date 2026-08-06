/**
 * @file opengl_text_ohos.cpp
 * @brief OpenGL 渲染器 OpenHarmony 文本管线与 EGL 上下文。
 * @author clk
 */

#if defined(__OHOS__)

#include <renderer/opengl_renderer.h>
#include <utils/console.h>

#include <native_drawing/drawing_bitmap.h>
#include <native_drawing/drawing_canvas.h>
#include <native_drawing/drawing_text_typography.h>
#include <native_drawing/drawing_font_collection.h>
#include <native_drawing/drawing_color.h>
#include <native_drawing/drawing_types.h>
#include <native_drawing/drawing_rect.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace spiration {

void opengl_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                                 float font_size, const std::string& font_family,
                                 bool word_wrap) {
    if (text.empty()) return;

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
        double wrap_width = static_cast<double>(width_ * dpi);
        if (position.x < static_cast<float>(width_)) {
            wrap_width = static_cast<double>((width_ - position.x) * dpi);
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

        glUseProgram(shape_shader_.program);
        glUniformMatrix4fv(shape_shader_.u_mvp, 1, GL_FALSE, proj_matrix_);
        glUniform1i(shape_shader_.u_use_texture, 1);
        glUniform1i(shape_shader_.u_texture_sampler, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        float a = text_color.a * alpha_;
        vertex verts[6] = {
            {x, y, 0, 0, text_color.r, text_color.g, text_color.b, a},
            {x + tw_logical, y, 1, 0, text_color.r, text_color.g, text_color.b, a},
            {x, y + th_logical, 0, 1, text_color.r, text_color.g, text_color.b, a},
            {x + tw_logical, y, 1, 0, text_color.r, text_color.g, text_color.b, a},
            {x + tw_logical, y + th_logical, 1, 1, text_color.r, text_color.g, text_color.b, a},
            {x, y + th_logical, 0, 1, text_color.r, text_color.g, text_color.b, a},
        };
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glUniform1i(shape_shader_.u_use_texture, 0);

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

void opengl_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
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

void opengl_renderer::set_density(float density) {
    density_ = density;
}

void opengl_renderer::set_physical_size(uint32_t width, uint32_t height) {
    physical_width_ = width;
    physical_height_ = height;
}

void opengl_renderer::set_logical_size(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    mat4_ortho(proj_matrix_, 0.0f, static_cast<float>(width_),
               static_cast<float>(height_), 0.0f);
}

void opengl_renderer::check_resize() {
    if (egl_display_ == EGL_NO_DISPLAY || egl_surface_ == EGL_NO_SURFACE) return;
    EGLint px_w = 0, px_h = 0;
    eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH, &px_w);
    eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &px_h);
    uint32_t uw = static_cast<uint32_t>(px_w);
    uint32_t uh = static_cast<uint32_t>(px_h);
    if (uw == physical_width_ && uh == physical_height_) return;
    physical_width_ = uw;
    physical_height_ = uh;
    if (uw == 0 || uh == 0) return;
    float dpi = density_ > 0.0f ? density_ : 1.0f;
    uint32_t vp_w = static_cast<uint32_t>(uw / dpi);
    uint32_t vp_h = static_cast<uint32_t>(uh / dpi);
    glViewport(0, 0, static_cast<GLsizei>(uw), static_cast<GLsizei>(uh));
    width_ = vp_w;
    height_ = vp_h;
    mat4_ortho(proj_matrix_, 0.0f, static_cast<float>(width_),
               static_cast<float>(height_), 0.0f);
}

bool opengl_renderer::init_egl(void* native_window) {
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
    width_ = static_cast<uint32_t>(w);
    height_ = static_cast<uint32_t>(h);
    return true;
}

void opengl_renderer::destroy_egl() {
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

float opengl_renderer::measure_text_width(const std::string& text, float font_size,
                                          const std::string& font_family) {
    if (text.empty()) return 0.0f;
    float dpi = density_ > 0.0f ? density_ : 1.0f;

    measure_cache_key key{text, font_family, font_size, -1.0f};
    auto cit = measure_cache_.find(key);
    if (cit != measure_cache_.end()) return cit->second;

    OH_Drawing_TypographyStyle* style = OH_Drawing_CreateTypographyStyle();
    OH_Drawing_SetTypographyTextDirection(style, TEXT_DIRECTION_LTR);
    OH_Drawing_SetTypographyTextAlign(style, TEXT_ALIGN_LEFT);
    OH_Drawing_FontCollection* fc = OH_Drawing_CreateFontCollection();
    OH_Drawing_TypographyCreate* handler = OH_Drawing_CreateTypographyHandler(style, fc);
    OH_Drawing_TextStyle* tStyle = OH_Drawing_CreateTextStyle();
    OH_Drawing_SetTextStyleFontSize(tStyle, static_cast<double>(font_size * dpi));
    if (!font_family.empty()) {
        const char* fam = font_family.c_str();
        OH_Drawing_SetTextStyleFontFamilies(tStyle, 1, &fam);
    }
    OH_Drawing_TypographyHandlerPushTextStyle(handler, tStyle);
    OH_Drawing_TypographyHandlerAddText(handler, text.c_str());
    OH_Drawing_Typography* typo = OH_Drawing_CreateTypography(handler);
    OH_Drawing_TypographyLayout(typo, 2000.0 * dpi);
    float w = static_cast<float>(OH_Drawing_TypographyGetLongestLine(typo)) / dpi;
    OH_Drawing_DestroyTypography(typo);
    OH_Drawing_DestroyTypographyHandler(handler);
    OH_Drawing_DestroyTextStyle(tStyle);
    OH_Drawing_DestroyFontCollection(fc);
    OH_Drawing_DestroyTypographyStyle(style);
    if (measure_cache_.size() >= MEASURE_CACHE_MAX) measure_cache_.clear();
    measure_cache_.emplace(std::move(key), w);
    return w;
}

float opengl_renderer::measure_text_height(const std::string& text, float font_size,
                                            const std::string& font_family,
                                            float wrap_width) {
    (void)font_family; (void)wrap_width;
    if (text.empty()) return 0.0f;
    return font_size;
}

} // namespace spiration

#endif // defined(__OHOS__)

