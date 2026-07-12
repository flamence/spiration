/**
 * @file opengl_renderer.cpp
 * @brief OpenGL 3.3 Core Profile 渲染器实现。
 * @author clk
 */

#include <renderer/opengl_renderer.h>
#include <utils/console.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef GLX_CONTEXT_MINOR_VERSION_ARB
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef GLX_CONTEXT_FLAGS_ARB
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#endif
#ifndef GLX_CONTEXT_PROFILE_MASK_ARB
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef GLX_CONTEXT_CORE_PROFILE_BIT_ARB
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

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
        console::error("OpenGL shader compilation failed (%d): %s", type, info);
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
        console::error("OpenGL program link failed: %s", info);
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

void opengl_renderer::mat4_multiply(float* result, const float* a, const float* b) {
    float tmp[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            tmp[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] +
                             a[i * 4 + 1] * b[1 * 4 + j] +
                             a[i * 4 + 2] * b[2 * 4 + j] +
                             a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
    std::memcpy(result, tmp, sizeof(tmp));
}

void opengl_renderer::mat4_ortho(float* m, float left, float right, float bottom, float top) {
    mat4_identity(m);
    m[0]  = 2.0f / (right - left);
    m[5]  = 2.0f / (top - bottom);
    m[10] = -1.0f;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
}

void opengl_renderer::mat4_translate(float* m, float x, float y, float z) {
    float t[16];
    mat4_identity(t);
    t[12] = x;
    t[13] = y;
    t[14] = z;
    float result[16];
    mat4_multiply(result, m, t);
    std::memcpy(m, result, sizeof(result));
}

void opengl_renderer::mat4_rotate_z(float* m, float angle_deg) {
    float rad = angle_deg * 3.14159265f / 180.0f;
    float c = std::cos(rad);
    float s = std::sin(rad);
    float r[16];
    mat4_identity(r);
    r[0] = c;  r[1] = s;
    r[4] = -s; r[5] = c;
    float result[16];
    mat4_multiply(result, m, r);
    std::memcpy(m, result, sizeof(result));
}

void opengl_renderer::mat4_scale(float* m, float sx, float sy, float sz) {
    float s[16];
    mat4_identity(s);
    s[0]  = sx;
    s[5]  = sy;
    s[10] = sz;
    float result[16];
    mat4_multiply(result, m, s);
    std::memcpy(m, result, sizeof(result));
}

opengl_renderer::opengl_renderer() {
    mat4_identity(current_transform_.m);
    mat4_identity(proj_matrix_);
    mat4_identity(mvp_matrix_);
}

opengl_renderer::~opengl_renderer() {
    shutdown();
}

opengl_renderer::opengl_renderer(opengl_renderer&& other) noexcept
    : display_(other.display_)
    , width_(other.width_)
    , height_(other.height_)
    , shape_shader_(other.shape_shader_)
    , vao_(other.vao_)
    , vbo_(other.vbo_)
    , batch_(std::move(other.batch_))
    , ft_library_(other.ft_library_)
    , font_faces_(std::move(other.font_faces_))
    , glyph_cache_(std::move(other.glyph_cache_))
    , glyph_atlas_(other.glyph_atlas_)
    , cjk_fallback_(other.cjk_fallback_)
    , images_(std::move(other.images_))
    , transform_stack_(std::move(other.transform_stack_))
    , current_transform_(other.current_transform_)
    , alpha_(other.alpha_)
    , blend_enabled_(other.blend_enabled_)
    , batch_has_texture_(other.batch_has_texture_)
    , current_texture_(other.current_texture_) {

    std::memcpy(proj_matrix_, other.proj_matrix_, sizeof(proj_matrix_));
    std::memcpy(mvp_matrix_, other.mvp_matrix_, sizeof(mvp_matrix_));

    other.display_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
    other.shape_shader_ = {};
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ft_library_ = nullptr;
    other.glyph_atlas_ = {};
    other.cjk_fallback_ = nullptr;
    other.alpha_ = 1.0f;
    other.blend_enabled_ = true;
    other.batch_has_texture_ = false;
    other.current_texture_ = 0;
    mat4_identity(other.current_transform_.m);
}

opengl_renderer& opengl_renderer::operator=(opengl_renderer&& other) noexcept {
    if (this != &other) {
        shutdown();

        display_ = other.display_;
        width_ = other.width_;
        height_ = other.height_;
        shape_shader_ = other.shape_shader_;
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        batch_ = std::move(other.batch_);
        ft_library_ = other.ft_library_;
        font_faces_ = std::move(other.font_faces_);
        glyph_cache_ = std::move(other.glyph_cache_);
        glyph_atlas_ = other.glyph_atlas_;
        cjk_fallback_ = other.cjk_fallback_;
        images_ = std::move(other.images_);
        transform_stack_ = std::move(other.transform_stack_);
        current_transform_ = other.current_transform_;
        alpha_ = other.alpha_;
        blend_enabled_ = other.blend_enabled_;
        batch_has_texture_ = other.batch_has_texture_;
        current_texture_ = other.current_texture_;

        std::memcpy(proj_matrix_, other.proj_matrix_, sizeof(proj_matrix_));
        std::memcpy(mvp_matrix_, other.mvp_matrix_, sizeof(mvp_matrix_));

        other.display_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;
        other.shape_shader_ = {};
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ft_library_ = nullptr;
        other.glyph_atlas_ = {};
        other.cjk_fallback_ = nullptr;
        other.alpha_ = 1.0f;
        other.blend_enabled_ = true;
        other.batch_has_texture_ = false;
        other.current_texture_ = 0;
        mat4_identity(other.current_transform_.m);
    }
    return *this;
}

bool opengl_renderer::initialize(void* native_window_handle) {
    display_ = static_cast<Display*>(native_window_handle);
    if (!display_) {
        console::error("opengl_renderer: invalid Display handle");
        return false;
    }

    width_ = 800;
    height_ = 600;

    if (!compile_shaders()) {
        console::error("opengl_renderer: failed to compile shaders");
        return false;
    }

    if (!create_buffers()) {
        console::error("opengl_renderer: failed to create buffers");
        return false;
    }

    if (!init_freetype()) {
        console::warning("opengl_renderer: FreeType init failed, text rendering disabled");
    } else {
        init_cjk_fallback_font();
    }

    init_glyph_atlas(&glyph_atlas_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4_ortho(proj_matrix_, 0.0f, static_cast<float>(width_),
               static_cast<float>(height_), 0.0f);

    console::info("opengl_renderer initialized (%ux%u)", width_, height_);
    return true;
}

void opengl_renderer::shutdown() {
    if (!batch_.empty()) {
        flush_batch();
    }

    for (auto& [path, img] : images_) {
        if (img.texture_id) glDeleteTextures(1, &img.texture_id);
    }
    images_.clear();

    if (glyph_atlas_.texture_id) {
        glDeleteTextures(1, &glyph_atlas_.texture_id);
        glyph_atlas_.texture_id = 0;
    }

    if (cjk_fallback_ && cjk_fallback_->face) {
        FT_Done_Face(cjk_fallback_->face);
        delete cjk_fallback_;
        cjk_fallback_ = nullptr;
    }
    for (auto& [key, face] : font_faces_) {
        if (face.face) FT_Done_Face(face.face);
    }
    font_faces_.clear();
    glyph_cache_.clear();

    if (ft_library_) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }

    destroy_buffers();
    destroy_shaders();

    display_ = nullptr;
    console::info("opengl_renderer shutdown");
}

void opengl_renderer::resize(uint32_t width, uint32_t height) {
    if (width_ == width && height_ == height) return;
    width_ = width;
    height_ = height;
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    mat4_ortho(proj_matrix_, 0.0f, static_cast<float>(width),
               static_cast<float>(height_), 0.0f);
}

void opengl_renderer::begin_frame() {
    batch_.clear();
    batch_.reserve(1024);
    batch_has_texture_ = false;
    current_texture_ = 0;

    transform_stack_.clear();
    mat4_identity(current_transform_.m);
    std::memcpy(mvp_matrix_, proj_matrix_, sizeof(mvp_matrix_));

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void opengl_renderer::end_frame() {
    if (!batch_.empty()) {
        flush_batch();
    }
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
    draw_circle_impl(center.x, center.y, radius, fill_color, true);
}

void opengl_renderer::draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width) {
    draw_circle_impl(center.x, center.y, radius, stroke_color, false);
}

void opengl_renderer::draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width) {
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

    if (batch_.size() >= MAX_BATCH_VERTICES) {
        flush_batch();
    }
}

void opengl_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                                 float font_size, const std::string& font_family) {
    if (!ft_library_ || text.empty()) return;

    font_face* fface = get_font_face(font_family.empty() ? "DejaVuSans" : font_family, font_size);
    if (!fface || !fface->face) {
        console::warning("draw_text: font face null for '%s'", font_family.c_str());
        return;
    }

    if (!batch_has_texture_ && !batch_.empty()) flush_batch();

    float r = text_color.r, g = text_color.g, b = text_color.b, a = text_color.a * alpha_;
    float cursor_x = position.x;
    float cursor_y = position.y;
    transform_point(cursor_x, cursor_y);

    size_t i = 0;
    while (i < text.size()) {
        char32_t codepoint;
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            codepoint = (c & 0x1F) << 6 | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            codepoint = (c & 0x0F) << 12 |
                        (static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6 |
                        (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            codepoint = (c & 0x07) << 18 |
                        (static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12 |
                        (static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6 |
                        (static_cast<unsigned char>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            ++i;
            continue;
        }

        if (codepoint == '\n') {
            cursor_x = position.x;
            cursor_y += font_size * 1.4f;
            continue;
        }

        glyph_info* gi = get_glyph(fface, codepoint);
        if (!gi) continue;

        float x0 = cursor_x + gi->bearing_x;
        float y0 = cursor_y - gi->bearing_y;
        float x1 = x0 + gi->width;
        float y1 = y0 + gi->height;

        batch_.push_back({x0, y0, gi->s0, gi->t0, r, g, b, a});
        batch_.push_back({x1, y0, gi->s1, gi->t0, r, g, b, a});
        batch_.push_back({x0, y1, gi->s0, gi->t1, r, g, b, a});
        batch_.push_back({x1, y0, gi->s1, gi->t0, r, g, b, a});
        batch_.push_back({x1, y1, gi->s1, gi->t1, r, g, b, a});
        batch_.push_back({x0, y1, gi->s0, gi->t1, r, g, b, a});
        batch_has_texture_ = true;

        cursor_x += gi->advance_x;

        if (batch_.size() >= MAX_BATCH_VERTICES) {
            flush_batch();
        }
    }
}

void opengl_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                                         text_alignment h_align, vertical_alignment v_align,
                                         float font_size, const std::string& font_family) {
    if (!ft_library_ || text.empty()) return;

    font_face* fface = get_font_face(font_family.empty() ? "DejaVuSans" : font_family, font_size);
    if (!fface || !fface->face) return;

    float text_width = measure_text_width(text, font_size, font_family);
    float text_height = font_size;

    float x = bounds.x;
    switch (h_align) {
        case text_alignment::left:   x = bounds.x; break;
        case text_alignment::center: x = bounds.x + (bounds.width - text_width) * 0.5f; break;
        case text_alignment::right:  x = bounds.x + bounds.width - text_width; break;
    }

    float y = bounds.y;
    switch (v_align) {
        case vertical_alignment::top:    y = bounds.y + font_size; break;
        case vertical_alignment::center: y = bounds.y + (bounds.height - text_height) * 0.5f + font_size; break;
        case vertical_alignment::bottom: y = bounds.y + bounds.height - text_height + font_size; break;
    }

    draw_text(text, point(x, y), text_color, font_size, font_family);
}

void opengl_renderer::draw_image(const std::string& image_path, const rectangle& destination) {
    image_resource* img = load_image(image_path);
    if (!img || !img->texture_id) return;

    glUseProgram(shape_shader_.program);
    glUniformMatrix4fv(shape_shader_.u_mvp, 1, GL_FALSE, mvp_matrix_);
    glUniform1i(shape_shader_.u_use_texture, 1);
    glUniform1i(shape_shader_.u_texture_sampler, 0);
    glUniform4f(shape_shader_.u_color, 1.0f, 1.0f, 1.0f, alpha_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, img->texture_id);

    float x = destination.x, y = destination.y;
    float w = destination.width, h = destination.height;

    vertex verts[6];
    verts[0] = {x,     y,     0, 0, 1, 1, 1, alpha_};
    verts[1] = {x + w, y,     1, 0, 1, 1, 1, alpha_};
    verts[2] = {x,     y + h, 0, 1, 1, 1, 1, alpha_};
    verts[3] = {x + w, y,     1, 0, 1, 1, 1, alpha_};
    verts[4] = {x + w, y + h, 1, 1, 1, 1, 1, alpha_};
    verts[5] = {x,     y + h, 0, 1, 1, 1, 1, alpha_};

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUniform1i(shape_shader_.u_use_texture, 0);
}

void opengl_renderer::draw_image_subregion(const std::string& image_path, const rectangle& source, const rectangle& destination) {
    image_resource* img = load_image(image_path);
    if (!img || !img->texture_id) return;

    float img_w = static_cast<float>(img->width);
    float img_h = static_cast<float>(img->height);

    float u0 = source.x / img_w;
    float v0 = source.y / img_h;
    float u1 = (source.x + source.width) / img_w;
    float v1 = (source.y + source.height) / img_h;

    glUseProgram(shape_shader_.program);
    glUniformMatrix4fv(shape_shader_.u_mvp, 1, GL_FALSE, mvp_matrix_);
    glUniform1i(shape_shader_.u_use_texture, 1);
    glUniform1i(shape_shader_.u_texture_sampler, 0);
    glUniform4f(shape_shader_.u_color, 1.0f, 1.0f, 1.0f, alpha_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, img->texture_id);

    float x = destination.x, y = destination.y;
    float w = destination.width, h = destination.height;

    vertex verts[6];
    verts[0] = {x,     y,     u0, v0, 1, 1, 1, alpha_};
    verts[1] = {x + w, y,     u1, v0, 1, 1, 1, alpha_};
    verts[2] = {x,     y + h, u0, v1, 1, 1, 1, alpha_};
    verts[3] = {x + w, y,     u1, v0, 1, 1, 1, alpha_};
    verts[4] = {x + w, y + h, u1, v1, 1, 1, 1, alpha_};
    verts[5] = {x,     y + h, u0, v1, 1, 1, 1, alpha_};

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUniform1i(shape_shader_.u_use_texture, 0);
}

void opengl_renderer::push_transform(float x, float y, float rotation, float scale_x, float scale_y) {
    transform_stack_.push_back(current_transform_);

    float local[16];
    mat4_identity(local);
    float rad = rotation * 3.14159265f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    local[0]  = scale_x * c;  local[1]  = scale_y * s;  local[4]  = -scale_x * s;
    local[5]  = scale_y * c;  local[12] = x;             local[13] = y;

    float result[16];
    mat4_multiply(result, local, current_transform_.m);
    std::memcpy(current_transform_.m, result, sizeof(result));
}

void opengl_renderer::pop_transform() {
    if (transform_stack_.empty()) return;
    current_transform_ = transform_stack_.back();
    transform_stack_.pop_back();
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
    alpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void opengl_renderer::get_viewport_size(uint32_t& width, uint32_t& height) const {
    width = width_;
    height = height_;
}

float opengl_renderer::measure_text_width(const std::string& text, float font_size,
                                           const std::string& font_family) {
    if (!ft_library_ || text.empty()) return 0.0f;

    font_face* fface = get_font_face(font_family.empty() ? "DejaVuSans" : font_family, font_size);
    if (!fface || !fface->face) return 0.0f;

    float width = 0.0f;
    size_t i = 0;
    while (i < text.size()) {
        char32_t codepoint;
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            codepoint = (c & 0x1F) << 6 | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            codepoint = (c & 0x0F) << 12 |
                        (static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6 |
                        (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            codepoint = (c & 0x07) << 18 |
                        (static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12 |
                        (static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6 |
                        (static_cast<unsigned char>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            ++i;
            continue;
        }

        if (codepoint == '\n') break;

        glyph_info* gi = get_glyph(fface, codepoint);
        if (gi) {
            width += gi->advance_x;
        } else {
            width += font_size * 0.5f;
        }
    }
    return width;
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
}

bool opengl_renderer::create_buffers() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    glBufferData(GL_ARRAY_BUFFER, MAX_BATCH_VERTICES * sizeof(vertex), nullptr, GL_DYNAMIC_DRAW);

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

    glUseProgram(shape_shader_.program);
    glUniformMatrix4fv(shape_shader_.u_mvp, 1, GL_FALSE, mvp_matrix_);

    if (batch_has_texture_) {
        glUniform1i(shape_shader_.u_use_texture, 1);
        glUniform1i(shape_shader_.u_texture_sampler, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glyph_atlas_.texture_id);
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
}

void opengl_renderer::apply_transform() {
    mat4_multiply(mvp_matrix_, proj_matrix_, current_transform_.m);
}

bool opengl_renderer::init_freetype() {
    if (FT_Init_FreeType(&ft_library_) != 0) {
        ft_library_ = nullptr;
        return false;
    }
    return true;
}

namespace {

std::string find_font_file(const char* const* paths) {
    for (const char* const* p = paths; *p; ++p) {
        if (access(*p, R_OK) == 0) {
            return *p;
        }
    }
    return {};
}

}

bool opengl_renderer::init_cjk_fallback_font() {
    if (!ft_library_) return false;
    const char* cjk_paths[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallback.ttf",
        nullptr
    };
    std::string path = find_font_file(cjk_paths);
    if (path.empty()) {
        console::warning("opengl_renderer: no CJK fallback font found");
        return false;
    }
    FT_Face ft_face = nullptr;
    if (FT_New_Face(ft_library_, path.c_str(), 0, &ft_face) != 0) {
        console::warning("opengl_renderer: failed to load CJK font: %s", path.c_str());
        return false;
    }
    cjk_fallback_ = new font_face{ft_face, 0.0f};
    console::info("opengl_renderer: loaded CJK fallback font from %s", path.c_str());
    return true;
}

opengl_renderer::font_face* opengl_renderer::get_font_face(const std::string& family, float size) {
    int int_size = static_cast<int>(std::round(size));
    std::string key = family + "_" + std::to_string(int_size);

    auto it = font_faces_.find(key);
    if (it != font_faces_.end()) return &it->second;

    if (!ft_library_) return nullptr;

    bool is_mono = (family == "DejaVuSansMono" || family == "Consolas" || family == "monospace");
    bool is_sans = !is_mono;

    const char* sans_paths[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttf",
        "/usr/share/fonts/truetype/droid/DroidSansFallback.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/wqy/wqy-microhei.ttf",
        "/usr/share/fonts/truetype/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        nullptr
    };
    const char* mono_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        nullptr
    };

    const char* const* paths = is_mono ? mono_paths : sans_paths;
    std::string font_path = find_font_file(paths);

    if (!font_path.empty()) {
        FT_New_Face(ft_library_, font_path.c_str(), 0, &font_faces_[key].face);
    }

    if (!font_faces_[key].face) {
        font_path = find_font_file(sans_paths);
        if (!font_path.empty()) {
            FT_New_Face(ft_library_, font_path.c_str(), 0, &font_faces_[key].face);
        }
    }

    if (!font_faces_[key].face) {
        console::warning("opengl_renderer: no font found for '%s'", family.c_str());
        font_faces_.erase(key);
        return nullptr;
    }

    console::info("opengl_renderer: loaded font '%s' from %s",
                  family.c_str(), font_path.c_str());

    font_faces_[key].size = size;
    FT_Set_Pixel_Sizes(font_faces_[key].face, 0, static_cast<FT_UInt>(size));
    return &font_faces_[key];
}

opengl_renderer::glyph_info* opengl_renderer::get_glyph(font_face* face, char32_t codepoint) {
    if (!face || !face->face) return nullptr;

    FT_Face active_face = face->face;
    FT_UInt glyph_index = FT_Get_Char_Index(active_face, codepoint);

    if (!glyph_index && cjk_fallback_ && cjk_fallback_->face) {
        active_face = cjk_fallback_->face;
        FT_Set_Pixel_Sizes(active_face, 0, static_cast<FT_UInt>(face->size));
        glyph_index = FT_Get_Char_Index(active_face, codepoint);
    }

    if (!glyph_index) return nullptr;

    uint64_t cache_key = (reinterpret_cast<uintptr_t>(active_face) << 1) ^
                         static_cast<uint64_t>(codepoint);

    auto it = glyph_cache_.find(cache_key);
    if (it != glyph_cache_.end()) return &it->second;

    if (!glyph_atlas_.texture_id) {
        if (!init_glyph_atlas(&glyph_atlas_)) return nullptr;
    }

    if (FT_Load_Glyph(active_face, glyph_index, FT_LOAD_RENDER)) return nullptr;

    FT_Bitmap* bitmap = &active_face->glyph->bitmap;
    int gw = bitmap->width;
    int gh = bitmap->rows;

    if (glyph_atlas_.cursor_x + gw + 1 >= glyph_atlas_.width) {
        glyph_atlas_.cursor_x = 1;
        glyph_atlas_.cursor_y += glyph_atlas_.row_height + 1;
        glyph_atlas_.row_height = 0;
    }
    if (glyph_atlas_.cursor_y + gh + 1 >= glyph_atlas_.height) {
        console::warning("opengl_renderer: glyph atlas full");
        return nullptr;
    }

    if (gh > glyph_atlas_.row_height) {
        glyph_atlas_.row_height = gh;
    }

    glBindTexture(GL_TEXTURE_2D, glyph_atlas_.texture_id);
    if (gw > 0 && gh > 0) {
        std::vector<uint8_t> rgba(gw * gh * 4);
        for (int row = 0; row < gh; ++row) {
            for (int col = 0; col < gw; ++col) {
                uint8_t pixel = bitmap->buffer[row * bitmap->pitch + col];
                int idx = (row * gw + col) * 4;
                rgba[idx + 0] = 0xFF;
                rgba[idx + 1] = 0xFF;
                rgba[idx + 2] = 0xFF;
                rgba[idx + 3] = pixel;
            }
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        glyph_atlas_.cursor_x, glyph_atlas_.cursor_y,
                        gw, gh, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }

    glyph_info gi;
    float atlas_w = static_cast<float>(glyph_atlas_.width);
    float atlas_h = static_cast<float>(glyph_atlas_.height);
    gi.s0 = static_cast<float>(glyph_atlas_.cursor_x) / atlas_w;
    gi.t0 = static_cast<float>(glyph_atlas_.cursor_y) / atlas_h;
    gi.s1 = static_cast<float>(glyph_atlas_.cursor_x + gw) / atlas_w;
    gi.t1 = static_cast<float>(glyph_atlas_.cursor_y + gh) / atlas_h;
    gi.bearing_x = static_cast<float>(active_face->glyph->bitmap_left);
    gi.bearing_y = static_cast<float>(active_face->glyph->bitmap_top);
    gi.advance_x = static_cast<float>(active_face->glyph->advance.x) / 64.0f;
    gi.width = static_cast<float>(gw);
    gi.height = static_cast<float>(gh);

    glyph_atlas_.cursor_x += gw + 1;

    auto result = glyph_cache_.emplace(cache_key, gi);
    return &result.first->second;
}

bool opengl_renderer::init_glyph_atlas(glyph_atlas* atlas) {
    if (atlas->texture_id) return true;

    glGenTextures(1, &atlas->texture_id);
    glBindTexture(GL_TEXTURE_2D, atlas->texture_id);

    std::vector<uint8_t> data(atlas->width * atlas->height * 4, 0);
    data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->width, atlas->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    atlas->cursor_x = 1;
    atlas->cursor_y = 1;
    atlas->row_height = 0;

    return true;
}

opengl_renderer::image_resource* opengl_renderer::load_image(const std::string& path) {
    auto it = images_.find(path);
    if (it != images_.end()) return &it->second;

    int w, h, channels;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        console::warning("opengl_renderer: failed to load image: %s", path.c_str());
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
    if (batch_has_texture_) flush_batch();

    float x1 = x, y1 = y;
    float x2 = x + w, y2 = y + h;
    transform_point(x1, y1);
    transform_point(x2, y2);
    w = x2 - x1;
    h = y2 - y1;

    float r = fill_color.r, g = fill_color.g, b = fill_color.b, a = fill_color.a * alpha_;
    push_rect_vertices(batch_, x1, y1, w, h, 0, 0, 0, 0, r, g, b, a);

    if (batch_.size() >= MAX_BATCH_VERTICES) {
        flush_batch();
    }
}

void opengl_renderer::draw_rect_outline_impl(float x, float y, float w, float h,
                                              const color& stroke_color, float stroke_width) {
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

    if (batch_.size() >= MAX_BATCH_VERTICES) {
        flush_batch();
    }
}

void opengl_renderer::draw_circle_impl(float cx, float cy, float radius, const color& fill_color, bool filled) {
    transform_point(cx, cy);
    float r = fill_color.r, g = fill_color.g, b = fill_color.b, a = fill_color.a * alpha_;

    if (filled) {
        float prev_x = cx + radius;
        float prev_y = cy;
        for (int i = 1; i <= CIRCLE_SEGMENTS; ++i) {
            float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
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
        float stroke_width = 1.0f;

        for (int i = 1; i <= CIRCLE_SEGMENTS; ++i) {
            float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
            float curr_x = cx + radius * std::cos(angle);
            float curr_y = cy + radius * std::sin(angle);

            float dx = curr_x - prev_x;
            float dy = curr_y - prev_y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f) {
                float nx = -dy / len * stroke_width * 0.5f;
                float ny =  dx / len * stroke_width * 0.5f;

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

    if (batch_.size() >= MAX_BATCH_VERTICES) {
        flush_batch();
    }
}

std::shared_ptr<renderer> renderer::create_opengl_renderer() {
    return std::make_shared<opengl_renderer>();
}

} // namespace spiration
