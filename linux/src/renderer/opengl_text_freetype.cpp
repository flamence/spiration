/**
 * @file opengl_text_freetype.cpp
 * @brief OpenGL 渲染器 FreeType 文本管线（Linux）。
 * @author clk
 */

#if !defined(__OHOS__)

#include <renderer/opengl_renderer.h>
#include <utils/console.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unistd.h>

namespace spiration {

void opengl_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                                 float font_size, const std::string& font_family,
                                 bool word_wrap) {
    if (!ft_library_ || text.empty()) return;

    font_face* fface = get_font_face(font_family.empty() ? "DejaVuSans" : font_family, font_size);
    if (!fface || !fface->face) {
        console::warning("text", "font face null for '%s'", font_family.c_str());
        return;
    }

    if (!batch_has_texture_ && !batch_.empty()) flush_batch();

    float r = text_color.r, g = text_color.g, b = text_color.b, a = text_color.a * alpha_;
    float ascent = static_cast<float>(fface->face->size->metrics.ascender) / 64.0f;
    float cursor_x = position.x;
    float cursor_y = position.y + ascent;
    transform_point(cursor_x, cursor_y);
    float line_start_x = cursor_x;

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
            transform_point(cursor_x, cursor_y);
            line_start_x = cursor_x;
            continue;
        }

        glyph_info* gi = get_glyph(fface, codepoint);
        if (!gi) continue;

        if (word_wrap && cursor_x + gi->advance_x > line_start_x + static_cast<float>(width_) &&
            cursor_x > line_start_x) {
            cursor_x = position.x;
            cursor_y += font_size * 1.4f;
            transform_point(cursor_x, cursor_y);
            line_start_x = cursor_x;
        }

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
    if (fface && fface->face && fface->face->size) {
        text_height = (static_cast<float>(fface->face->size->metrics.ascender) -
                       static_cast<float>(fface->face->size->metrics.descender)) / 64.0f;
    }

    float x = bounds.x;
    switch (h_align) {
        case text_alignment::left:   x = bounds.x; break;
        case text_alignment::center: x = bounds.x + (bounds.width - text_width) * 0.5f; break;
        case text_alignment::right:  x = bounds.x + bounds.width - text_width; break;
    }

    float y = bounds.y;
    switch (v_align) {
        case vertical_alignment::top:    y = bounds.y; break;
        case vertical_alignment::center: y = bounds.y + (bounds.height - text_height) * 0.5f; break;
        case vertical_alignment::bottom: y = bounds.y + bounds.height - text_height; break;
    }

    draw_text(text, point(x, y), text_color, font_size, font_family);
}

float opengl_renderer::measure_text_width(const std::string& text, float font_size,
                                          const std::string& font_family) {
    if (!ft_library_ || text.empty()) return 0.0f;

    // 测量缓存：布局/重排阶段会高频重复测量同一文本，命中后免去逐字形迭代。
    measure_cache_key key{text, font_family, font_size, -1.0f};
    auto cit = measure_cache_.find(key);
    if (cit != measure_cache_.end()) return cit->second;

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

        // 仅取字形 advance（不触发光栅化与纹理上传），避免布局期卡顿。
        float adv = get_glyph_advance(fface, codepoint);
        width += (adv >= 0.0f) ? adv : font_size * 0.5f;
    }

    if (measure_cache_.size() >= MEASURE_CACHE_MAX) measure_cache_.clear();
    measure_cache_.emplace(std::move(key), width);
    return width;
}

float opengl_renderer::measure_text_height(const std::string& text, float font_size,
                                            const std::string& font_family,
                                            float wrap_width) {
    if (!ft_library_ || text.empty()) return 0.0f;

    measure_cache_key key{text, font_family, font_size, wrap_width};
    auto cit = measure_cache_.find(key);
    if (cit != measure_cache_.end()) return cit->second;

    font_face* fface = get_font_face(font_family.empty() ? "DejaVuSans" : font_family, font_size);
    if (!fface || !fface->face) return 0.0f;

    float line_width = 0.0f;
    float lines = 1.0f;
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
            ++lines;
            line_width = 0.0f;
            continue;
        }

        // 仅取字形 advance（不触发光栅化与纹理上传），避免布局期卡顿。
        float adv = get_glyph_advance(fface, codepoint);
        if (adv < 0.0f) adv = font_size * 0.5f;

        if (wrap_width > 0.0f && line_width + adv > wrap_width && line_width > 0.0f) {
            ++lines;
            line_width = adv;
        } else {
            line_width += adv;
        }
    }

    float result = lines * font_size * 1.4f;
    if (measure_cache_.size() >= MEASURE_CACHE_MAX) measure_cache_.clear();
    measure_cache_.emplace(std::move(key), result);
    return result;
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
        console::warning("renderer/opengl", "no CJK fallback font found");
        return false;
    }
    FT_Face ft_face = nullptr;
    if (FT_New_Face(ft_library_, path.c_str(), 0, &ft_face) != 0) {
        console::warning("renderer/opengl", "failed to load CJK font: %s", path.c_str());
        return false;
    }
    cjk_fallback_ = new font_face{ft_face, 0.0f};
    console::info("renderer/opengl", "loaded CJK fallback font from %s", path.c_str());
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
        console::warning("renderer/opengl", "no font found for '%s'", family.c_str());
        font_faces_.erase(key);
        return nullptr;
    }

    console::info("renderer/opengl", "loaded font '%s' from %s",
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
        // 先提交当前批次（其 UV 引用旧图集），再清缓存重建，避免同帧新旧 UV 混乱
        flush_batch();
        glyph_cache_.clear();
        glDeleteTextures(1, &glyph_atlas_.texture_id);
        glyph_atlas_.texture_id = 0;
        glyph_atlas_.cursor_x = 1;
        glyph_atlas_.cursor_y = 1;
        glyph_atlas_.row_height = 0;
        if (!init_glyph_atlas(&glyph_atlas_)) return nullptr;
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

float opengl_renderer::get_glyph_advance(font_face* face, char32_t codepoint) {
    if (!face || !face->face) return -1.0f;

    FT_Face active_face = face->face;
    FT_UInt glyph_index = FT_Get_Char_Index(active_face, codepoint);

    if (!glyph_index && cjk_fallback_ && cjk_fallback_->face) {
        active_face = cjk_fallback_->face;
        FT_Set_Pixel_Sizes(active_face, 0, static_cast<FT_UInt>(face->size));
        glyph_index = FT_Get_Char_Index(active_face, codepoint);
    }

    if (!glyph_index) return -1.0f;

    uint64_t cache_key = (reinterpret_cast<uintptr_t>(active_face) << 1) ^
                         static_cast<uint64_t>(codepoint);

    auto it = glyph_advance_cache_.find(cache_key);
    if (it != glyph_advance_cache_.end()) return it->second;

    // 仅取字形度量（advance）：不执行 FT_LOAD_RENDER，不做光栅化与纹理上传。
    if (FT_Load_Glyph(active_face, glyph_index, FT_LOAD_DEFAULT)) return -1.0f;

    float advance = static_cast<float>(active_face->glyph->advance.x) / 64.0f;
    glyph_advance_cache_.emplace(cache_key, advance);
    return advance;
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

} // namespace spiration

#endif // !defined(__OHOS__)

