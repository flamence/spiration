/**
 * @file opengl_text_coretext.mm
 * @brief OpenGL 渲染器 macOS 文本管线：基于 CoreText 原生文本绘制。
 *        零第三方依赖（仅系统框架），兼容 macOS 10.15。
 * @author clk
 */

#if defined(__APPLE__)

#include <spiration/renderer/opengl_renderer.h>

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include <cmath>
#include <vector>

namespace spiration {

namespace {

/// 将 UTF-8 字节串安全转为 NSString。
/// stringWithUTF8String: 遇到非法 UTF-8（如流式输出被分块截断的多字节序列）返回 nil，
/// 若直接用于 NSAttributedString 会崩溃（NSInvalidArgumentException nil value）。
/// 这里逐字节校验，非法字节以 U+FFFD 替换，保证返回值永不为 nil。
NSString* ns_string_from_utf8(const std::string& text) {
    NSString* ns = [NSString stringWithUTF8String:text.c_str()];
    if (ns) return ns;

    NSMutableString* out = [NSMutableString stringWithCapacity:text.size()];
    const unsigned char* p = reinterpret_cast<const unsigned char*>(text.data());
    size_t n = text.size();
    size_t i = 0;
    while (i < n) {
        unsigned char c = p[i];
        size_t len = 0;
        if (c < 0x80) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        bool valid = len > 0 && i + len <= n;
        if (valid) {
            for (size_t k = 1; k < len; ++k) {
                if ((p[i + k] & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
            }
        }
        if (valid) {
            NSString* piece = [[NSString alloc] initWithBytes:p + i
                                                       length:len
                                                     encoding:NSUTF8StringEncoding];
            if (piece) {
                [out appendString:piece];
                i += len;
                continue;
            }
        }
        [out appendString:@"\uFFFD"];
        ++i;
    }
    return out;
}

CTFontRef create_ct_font(const std::string& family, float size) {
    NSString* ns_family = family.empty() ? @"Helvetica" : ns_string_from_utf8(family);
    CTFontRef font = CTFontCreateWithName((__bridge CFStringRef)ns_family, size, nullptr);
    if (!font) {
        // 回退系统默认字体（CoreText 在绘制时仍会按需级联到中日韩字体）。
        font = CTFontCreateWithName(CFSTR("Helvetica"), size, nullptr);
    }
    return font;
}

NSAttributedString* create_attributed_string(const std::string& text, CTFontRef font) {
    NSString* ns_text = ns_string_from_utf8(text);
    NSDictionary* attrs = @{
        (__bridge id)kCTFontAttributeName: (__bridge id)font,
        (__bridge id)kCTForegroundColorAttributeName:
            (__bridge id)CGColorGetConstantColor(kCGColorWhite)
    };
    return [[NSAttributedString alloc] initWithString:ns_text attributes:attrs];
}

} // namespace

float opengl_renderer::measure_text_width(const std::string& text, float font_size,
                                          const std::string& font_family) {
    if (text.empty()) return 0.0f;

    measure_cache_key key{text, font_family, font_size, -1.0f};
    auto cit = measure_cache_.find(key);
    if (cit != measure_cache_.end()) return cit->second;

    float width = 0.0f;
    @autoreleasepool {
        CTFontRef font = create_ct_font(font_family, font_size);
        if (!font) return 0.0f;

        NSAttributedString* attr_str = create_attributed_string(text, font);
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)attr_str);
        width = static_cast<float>(CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr));
        CFRelease(line);
        CFRelease(font);
    }

    if (measure_cache_.size() >= MEASURE_CACHE_MAX) measure_cache_.clear();
    measure_cache_.emplace(std::move(key), width);
    return width;
}

float opengl_renderer::measure_text_height(const std::string& text, float font_size,
                                            const std::string& font_family,
                                            float wrap_width) {
    if (text.empty()) return 0.0f;

    measure_cache_key key{text, font_family, font_size, wrap_width};
    auto cit = measure_cache_.find(key);
    if (cit != measure_cache_.end()) return cit->second;

    float height = 0.0f;
    @autoreleasepool {
        CTFontRef font = create_ct_font(font_family, font_size);
        if (!font) return 0.0f;

        NSAttributedString* attr_str = create_attributed_string(text, font);
        CTFramesetterRef framesetter =
            CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)attr_str);
        CGFloat wrap = wrap_width > 0.0f ? static_cast<CGFloat>(wrap_width) : 10000.0f;
        CGSize constraints = CGSizeMake(wrap, CGFLOAT_MAX);
        CGSize suggested = CTFramesetterSuggestFrameSizeWithConstraints(
            framesetter, CFRangeMake(0, 0), nullptr, constraints, nullptr);
        height = static_cast<float>(suggested.height);
        CFRelease(framesetter);
        CFRelease(font);
    }

    if (measure_cache_.size() >= MEASURE_CACHE_MAX) measure_cache_.clear();
    measure_cache_.emplace(std::move(key), height);
    return height;
}

void opengl_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                                 float font_size, const std::string& font_family,
                                 bool word_wrap) {
    if (text.empty()) return;

    @autoreleasepool {
        CTFontRef font = create_ct_font(font_family, font_size);
        if (!font) return;

        NSAttributedString* attr_str = create_attributed_string(text, font);

        CGFloat text_width = 0.0f, text_height = 0.0f;
        CGContextRef bitmap_ctx = nullptr;

        if (word_wrap) {
            CTFramesetterRef framesetter =
                CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)attr_str);

            CGFloat wrap_width = static_cast<CGFloat>(width_) - position.x;
            if (wrap_width < 10.0f) wrap_width = static_cast<CGFloat>(width_);
            CGSize constraints = CGSizeMake(wrap_width, CGFLOAT_MAX);
            CGSize suggested = CTFramesetterSuggestFrameSizeWithConstraints(
                framesetter, CFRangeMake(0, 0), nullptr, constraints, nullptr);

            text_width = std::ceil(suggested.width);
            text_height = std::ceil(suggested.height);
            if (text_width < 1.0f) text_width = 1.0f;
            if (text_height < 1.0f) text_height = 1.0f;

            // RGBA（非预乘字节序下的内存布局），行 0 为图像顶部
            CGBitmapInfo bitmap_info = (CGBitmapInfo)kCGImageAlphaPremultipliedLast;
            bitmap_ctx = CGBitmapContextCreate(
                nullptr, (size_t)text_width, (size_t)text_height, 8,
                (size_t)text_width * 4, CGColorSpaceCreateDeviceRGB(), bitmap_info);

            if (bitmap_ctx) {
                CGMutablePathRef path = CGPathCreateMutable();
                CGPathAddRect(path, nullptr, CGRectMake(0, 0, text_width, text_height));
                CTFrameRef frame = CTFramesetterCreateFrame(
                    framesetter, CFRangeMake(0, 0), path, nullptr);
                CTFrameDraw(frame, bitmap_ctx);
                CFRelease(frame);
                CFRelease(path);
            }
            CFRelease(framesetter);
        } else {
            CTLineRef line = CTLineCreateWithAttributedString(
                (__bridge CFAttributedStringRef)attr_str);

            CGFloat ascent = 0.0f, descent = 0.0f, leading = 0.0f;
            text_width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
            text_height = ascent + descent + leading;
            if (text_width < 1.0f) text_width = 1.0f;
            if (text_height < 1.0f) text_height = 1.0f;

            CGBitmapInfo bitmap_info = (CGBitmapInfo)kCGImageAlphaPremultipliedLast;
            bitmap_ctx = CGBitmapContextCreate(
                nullptr, (size_t)std::ceil(text_width), (size_t)std::ceil(text_height), 8,
                (size_t)std::ceil(text_width) * 4, CGColorSpaceCreateDeviceRGB(), bitmap_info);

            if (bitmap_ctx) {
                // 默认（未翻转）CTM：基线位于 y=descent，内存行 0 对应文本顶部
                CGContextSetTextPosition(bitmap_ctx, 0, descent);
                CTLineDraw(line, bitmap_ctx);
            }
            CFRelease(line);
        }

        if (bitmap_ctx) {
            size_t w = CGBitmapContextGetWidth(bitmap_ctx);
            size_t h = CGBitmapContextGetHeight(bitmap_ctx);
            size_t bpr = CGBitmapContextGetBytesPerRow(bitmap_ctx);
            const uint8_t* pixels = static_cast<const uint8_t*>(CGBitmapContextGetData(bitmap_ctx));

            // 位图内存行 0 即图像顶部（与 Metal 渲染器同一约定）；
            // 纹理行 0 对应四边形顶部（v=0），因此按序拷贝、不得翻转，
            // 否则字形会上下颠倒。
            std::vector<uint8_t> rgba(w * h * 4, 0xFF);
            for (size_t row = 0; row < h; ++row) {
                const uint8_t* src = pixels + row * bpr;
                for (size_t col = 0; col < w; ++col) {
                    rgba[(row * w + col) * 4 + 3] = src[col * 4 + 3];
                }
            }

            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            if (!batch_.empty()) flush_batch();

            float x = position.x, y = position.y;
            transform_point(x, y);
            float ww = static_cast<float>(text_width);
            float hh = static_cast<float>(text_height);
            float r = text_color.r, g = text_color.g, b = text_color.b;
            float a = text_color.a * alpha_;

            glUseProgram(shape_shader_.program);
            glUniformMatrix4fv(shape_shader_.u_mvp, 1, GL_FALSE, mvp_matrix_);
            glUniform1i(shape_shader_.u_use_texture, 1);
            glUniform1i(shape_shader_.u_texture_sampler, 0);
            glUniform4f(shape_shader_.u_color, 1.0f, 1.0f, 1.0f, alpha_);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);

            vertex verts[6] = {
                {x,     y,     0, 0, r, g, b, a},
                {x + ww, y,    1, 0, r, g, b, a},
                {x,     y + hh, 0, 1, r, g, b, a},
                {x + ww, y,    1, 0, r, g, b, a},
                {x + ww, y + hh, 1, 1, r, g, b, a},
                {x,     y + hh, 0, 1, r, g, b, a},
            };

            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glUniform1i(shape_shader_.u_use_texture, 0);
            glDeleteTextures(1, &tex);
            CGContextRelease(bitmap_ctx);
        }
        CFRelease(font);
    }
}

void opengl_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                                         text_alignment h_align, vertical_alignment v_align,
                                         float font_size, const std::string& font_family) {
    if (text.empty()) return;

    float text_width = measure_text_width(text, font_size, font_family);
    float text_height = measure_text_height(text, font_size, font_family, bounds.width);

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

} // namespace spiration

#endif // defined(__APPLE__)
