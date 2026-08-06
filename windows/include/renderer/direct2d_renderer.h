/**
 * @file direct2d_renderer.h
 * @brief Direct2D 渲染器实现。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>

namespace spiration {

/**
 * @brief Direct2D 渲染器实现。
 */
class direct2d_renderer : public renderer {
public:
    direct2d_renderer();
    ~direct2d_renderer() override;
    
    direct2d_renderer(const direct2d_renderer&) = delete;
    direct2d_renderer& operator=(const direct2d_renderer&) = delete;
    
    direct2d_renderer(direct2d_renderer&& other) noexcept;
    direct2d_renderer& operator=(direct2d_renderer&& other) noexcept;
    
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

    float measure_text_width(const std::string& text, float font_size = 16.0f,
                             const std::string& font_family = "Arial") override;
    float measure_text_height(const std::string& text, float font_size = 16.0f,
                              const std::string& font_family = "Arial",
                              float wrap_width = 10000.0f) override;
    
private:
    
    bool create_device_resources();
    void release_device_resources();
    float get_dpi() const;
    
    Microsoft::WRL::ComPtr<ID2D1Factory> create_d2d_factory();
    Microsoft::WRL::ComPtr<IDWriteFactory> create_dwrite_factory();
    Microsoft::WRL::ComPtr<IWICImagingFactory> create_wic_factory();
    
    
    struct texture_resource {
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    
    struct font_resource {
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        float size = 0.0f;
    };
    
    texture_resource* load_texture(const std::string& path);
    font_resource* get_font(const std::string& family, float size,
                            DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                            DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL,
                            DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL);
    
    
    D2D1_COLOR_F to_d2d_color(const color& c) const;
    D2D1_RECT_F to_d2d_rect(const rectangle& r) const;
    D2D1_POINT_2F to_d2d_point(const point& p) const;
    
    
    struct transform {
        D2D1_MATRIX_3X2_F matrix;
    };
    
private:
    HWND m_hWnd = nullptr;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    
    Microsoft::WRL::ComPtr<ID2D1Factory> m_D2DFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_DWriteFactory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_WICFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_RenderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_Brush;
    
    std::unordered_map<std::string, texture_resource> m_Textures;
    std::unordered_map<std::string, font_resource> m_Fonts;

    struct measure_key {
        std::string text;
        float size = 0.0f;
        float wrap = 0.0f;
        std::string family;
        bool operator<(const measure_key& o) const {
            if (size != o.size) return size < o.size;
            if (wrap != o.wrap) return wrap < o.wrap;
            if (family != o.family) return family < o.family;
            return text < o.text;
        }
    };
    std::map<measure_key, float> m_MeasureCache;
    static constexpr size_t MEASURE_CACHE_MAX = 16384;
    
    std::vector<transform> m_TransformStack;
    transform m_CurrentTransform;
    
    float m_Alpha = 1.0f;
    bool m_BlendEnabled = true;
    bool m_DeviceLost = false;
    bool m_ComInitialized = false;
    
    static constexpr wchar_t DEFAULT_FONT[] = L"Arial";
};

} 
