/**
 * @file opengl_text_freetype.cpp
 * @brief OpenGL FreeType 文本管线：Linux 字体发现实现。
 *        公共管线见 common/src/spiration/renderer/opengl_text_freetype.cpp。
 * @author clk
 */

#if defined(__linux__) && !defined(__OHOS__)

#include <spiration/application.h>
#include <spiration/renderer/opengl_renderer.h>
#include <cmath>
#include <fmt/format.h>

namespace spiration {

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
        application::instance()->logger()->log({logging::level::warning, "no CJK fallback font found", "renderer/opengl"});
        return false;
    }
    FT_Face ft_face = nullptr;
    if (FT_New_Face(ft_library_, path.c_str(), 0, &ft_face) != 0) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("failed to load CJK font: {}", path), "renderer/opengl"});
        return false;
    }
    cjk_fallback_ = new font_face{ft_face, 0.0f};
    application::instance()->logger()->log({logging::level::info, fmt::format("loaded CJK fallback font from {}", path), "renderer/opengl"});
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
        application::instance()->logger()->log({logging::level::warning, fmt::format("no font found for '{}'", family), "renderer/opengl"});
        font_faces_.erase(key);
        return nullptr;
    }

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded font '{}' from {}", family, font_path), "renderer/opengl"});

    font_faces_[key].size = size;
    FT_Set_Pixel_Sizes(font_faces_[key].face, 0, static_cast<FT_UInt>(size));
    return &font_faces_[key];
}

} // namespace spiration

#endif // defined(__linux__) && !defined(__OHOS__)
