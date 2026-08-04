/**
 * @file direct2d_renderer.cpp
 * @brief Direct2D 渲染器实现。
 * @author clk
 */

#include <renderer/direct2d_renderer.h>
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <comdef.h>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cmath>
#include <cassert>

#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#endif

using namespace Microsoft::WRL;

namespace spiration {


static std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    if (size_needed == 0) return L"";
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
    return wstr;
}

[[maybe_unused]] static std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed == 0) return "";
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
    return str;
}


direct2d_renderer::direct2d_renderer() = default;

direct2d_renderer::~direct2d_renderer() {
    shutdown();
}

direct2d_renderer::direct2d_renderer(direct2d_renderer&& other) noexcept
    : m_hWnd(other.m_hWnd)
    , m_Width(other.m_Width)
    , m_Height(other.m_Height)
    , m_D2DFactory(std::move(other.m_D2DFactory))
    , m_DWriteFactory(std::move(other.m_DWriteFactory))
    , m_WICFactory(std::move(other.m_WICFactory))
    , m_RenderTarget(std::move(other.m_RenderTarget))
    , m_Brush(std::move(other.m_Brush))
    , m_Textures(std::move(other.m_Textures))
    , m_Fonts(std::move(other.m_Fonts))
    , m_TransformStack(std::move(other.m_TransformStack))
    , m_CurrentTransform(other.m_CurrentTransform)
    , m_Alpha(other.m_Alpha)
    , m_BlendEnabled(other.m_BlendEnabled)
    , m_DeviceLost(other.m_DeviceLost) {
    
    other.m_hWnd = nullptr;
    other.m_Width = 0;
    other.m_Height = 0;
    other.m_Alpha = 1.0f;
    other.m_BlendEnabled = true;
    other.m_DeviceLost = false;
}

direct2d_renderer& direct2d_renderer::operator=(direct2d_renderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        
        m_hWnd = other.m_hWnd;
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_D2DFactory = std::move(other.m_D2DFactory);
        m_DWriteFactory = std::move(other.m_DWriteFactory);
        m_WICFactory = std::move(other.m_WICFactory);
        m_RenderTarget = std::move(other.m_RenderTarget);
        m_Brush = std::move(other.m_Brush);
        m_Textures = std::move(other.m_Textures);
        m_Fonts = std::move(other.m_Fonts);
        m_TransformStack = std::move(other.m_TransformStack);
        m_CurrentTransform = other.m_CurrentTransform;
        m_Alpha = other.m_Alpha;
        m_BlendEnabled = other.m_BlendEnabled;
        m_DeviceLost = other.m_DeviceLost;
        
        other.m_hWnd = nullptr;
        other.m_Width = 0;
        other.m_Height = 0;
        other.m_Alpha = 1.0f;
        other.m_BlendEnabled = true;
        other.m_DeviceLost = false;
    }
    return *this;
}

bool direct2d_renderer::initialize(void* native_window_handle) {
    m_hWnd = static_cast<HWND>(native_window_handle);
    if (!m_hWnd || !IsWindow(m_hWnd)) {
        return false;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
        return false;
    }
    
    m_D2DFactory = create_d2d_factory();
    if (!m_D2DFactory) return false;
    
    m_DWriteFactory = create_dwrite_factory();
    if (!m_DWriteFactory) return false;
    
    m_WICFactory = create_wic_factory();
    if (!m_WICFactory) return false;
    
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    float dpi = get_dpi();
    m_Width = static_cast<uint32_t>((rc.right - rc.left) * 96.0f / dpi);
    m_Height = static_cast<uint32_t>((rc.bottom - rc.top) * 96.0f / dpi);
    
    if (!create_device_resources()) {
        return false;
    }
    
    m_CurrentTransform.matrix = D2D1::Matrix3x2F::Identity();
    return true;
}

void direct2d_renderer::shutdown() {
    release_device_resources();
    m_Fonts.clear();
    m_Textures.clear();
    m_MeasureCache.clear();
    m_WICFactory.Reset();
    m_DWriteFactory.Reset();
    m_D2DFactory.Reset();
    m_hWnd = nullptr;
}

void direct2d_renderer::resize(uint32_t width, uint32_t height) {
    if (m_Width == width && m_Height == height) return;
    
    m_Width = width;
    m_Height = height;
    
    if (m_RenderTarget) {
        float dpi = get_dpi();
        UINT pw = static_cast<UINT>(std::lround(width * dpi / 96.0f));
        UINT ph = static_cast<UINT>(std::lround(height * dpi / 96.0f));
        m_RenderTarget->Resize(D2D1::SizeU(pw, ph));
        
        m_RenderTarget->SetDpi(dpi, dpi);
    }
}


void direct2d_renderer::begin_frame() {
    if (m_DeviceLost) {
        if (!create_device_resources()) {
            return;
        }
        m_DeviceLost = false;
    }
    if (m_RenderTarget) {
        m_RenderTarget->BeginDraw();
        m_RenderTarget->SetTransform(m_CurrentTransform.matrix);
        m_RenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
}

void direct2d_renderer::end_frame() {
    if (m_RenderTarget) {
        HRESULT hr = m_RenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            m_DeviceLost = true;
            release_device_resources();
        }
    }
}

void direct2d_renderer::clear(const color& clear_color) {
    if (m_RenderTarget) {
        m_RenderTarget->Clear(to_d2d_color(clear_color));
    }
}


void direct2d_renderer::draw_rectangle(const rectangle& rectangle, const color& fill_color) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(fill_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    m_RenderTarget->FillRectangle(to_d2d_rect(rectangle), m_Brush.Get());
}

void direct2d_renderer::draw_rectangle_outline(const rectangle& rectangle, const color& stroke_color, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    m_RenderTarget->DrawRectangle(to_d2d_rect(rectangle), m_Brush.Get(), stroke_width);
}

void direct2d_renderer::draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(fill_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(to_d2d_rect(rect), radius, radius);
    m_RenderTarget->FillRoundedRectangle(rr, m_Brush.Get());
}

void direct2d_renderer::draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(to_d2d_rect(rect), radius, radius);
    m_RenderTarget->DrawRoundedRectangle(rr, m_Brush.Get(), stroke_width);
}

void direct2d_renderer::push_clip(const rectangle& rect) {
    if (!m_RenderTarget) return;
    m_RenderTarget->PushAxisAlignedClip(to_d2d_rect(rect), D2D1_ANTIALIAS_MODE_ALIASED);
}

void direct2d_renderer::pop_clip() {
    if (!m_RenderTarget) return;
    m_RenderTarget->PopAxisAlignedClip();
}

void direct2d_renderer::draw_circle(const point& center, float radius, const color& fill_color) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(fill_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(to_d2d_point(center), radius, radius);
    m_RenderTarget->FillEllipse(ellipse, m_Brush.Get());
}

void direct2d_renderer::draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(to_d2d_point(center), radius, radius);
    m_RenderTarget->DrawEllipse(ellipse, m_Brush.Get(), stroke_width);
}

void direct2d_renderer::draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    m_RenderTarget->DrawLine(to_d2d_point(start), to_d2d_point(end), m_Brush.Get(), stroke_width);
}


void direct2d_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                                  float font_size, const std::string& font_family,
                                  bool word_wrap) {
    if (!m_RenderTarget || !m_DWriteFactory) return;
    
    font_resource* font = get_font(font_family.empty() ? "Arial" : font_family, font_size);
    if (!font || !font->format) return;
    
    D2D1_COLOR_F color = to_d2d_color(text_color);
    color.a *= m_Alpha;
    if (!m_Brush) return;
    m_Brush->SetColor(color);
    
    std::wstring wtext = string_to_wstring(text);
    
    if (word_wrap) {
        D2D1_RECT_F layout_rect = D2D1::RectF(position.x, position.y,
                                              position.x + static_cast<float>(m_Width),
                                              position.y + static_cast<float>(m_Height));
        m_RenderTarget->DrawTextW(wtext.c_str(), static_cast<UINT32>(wtext.length()),
                                  font->format.Get(), layout_rect, m_Brush.Get());
    } else {
        ComPtr<IDWriteTextLayout> layout;
        if (SUCCEEDED(m_DWriteFactory->CreateTextLayout(
                wtext.c_str(), static_cast<UINT32>(wtext.length()),
                font->format.Get(), 10000.0f, 100.0f, &layout))) {
            layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_RenderTarget->DrawTextLayout(D2D1::Point2F(position.x, position.y),
                                           layout.Get(), m_Brush.Get());
        }
    }
}

void direct2d_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                                          text_alignment h_align, vertical_alignment v_align,
                                          float font_size, const std::string& font_family) {
    if (!m_RenderTarget || !m_DWriteFactory) return;
    
    font_resource* font = get_font(font_family.empty() ? "Arial" : font_family, font_size);
    if (!font || !font->format) return;
    
    
    std::wstring wtext = string_to_wstring(text);
    ComPtr<IDWriteTextLayout> textLayout;
    HRESULT hr = m_DWriteFactory->CreateTextLayout(
        wtext.c_str(), static_cast<UINT32>(wtext.length()),
        font->format.Get(),
        bounds.width, bounds.height,
        &textLayout);
    if (FAILED(hr)) return;
    
    
    DWRITE_TEXT_ALIGNMENT dwriteHA = DWRITE_TEXT_ALIGNMENT_LEADING;
    DWRITE_PARAGRAPH_ALIGNMENT dwriteVA = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    switch (h_align) {
        case text_alignment::left:   dwriteHA = DWRITE_TEXT_ALIGNMENT_LEADING; break;
        case text_alignment::center: dwriteHA = DWRITE_TEXT_ALIGNMENT_CENTER;  break;
        case text_alignment::right:  dwriteHA = DWRITE_TEXT_ALIGNMENT_TRAILING; break;
    }
    switch (v_align) {
        case vertical_alignment::top:    dwriteVA = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;   break;
        case vertical_alignment::center: dwriteVA = DWRITE_PARAGRAPH_ALIGNMENT_CENTER; break;
        case vertical_alignment::bottom: dwriteVA = DWRITE_PARAGRAPH_ALIGNMENT_FAR;    break;
    }
    textLayout->SetTextAlignment(dwriteHA);
    textLayout->SetParagraphAlignment(dwriteVA);
    
    D2D1_COLOR_F color = to_d2d_color(text_color);
    color.a *= m_Alpha;
    if (!m_Brush) return;
    m_Brush->SetColor(color);
    
    D2D1_POINT_2F origin = D2D1::Point2F(bounds.x, bounds.y);
    m_RenderTarget->DrawTextLayout(origin, textLayout.Get(), m_Brush.Get());
}


void direct2d_renderer::draw_image(const std::string& image_path, const rectangle& destination) {
    if (!m_RenderTarget || !m_WICFactory) return;
    texture_resource* texture = load_texture(image_path);
    if (!texture || !texture->bitmap) return;
    m_RenderTarget->DrawBitmap(texture->bitmap.Get(), to_d2d_rect(destination), m_Alpha);
}

void direct2d_renderer::draw_image_subregion(const std::string& image_path, const rectangle& source, const rectangle& destination) {
    if (!m_RenderTarget || !m_WICFactory) return;
    texture_resource* texture = load_texture(image_path);
    if (!texture || !texture->bitmap) return;
    
    
    float srcLeft   = (std::max)(source.x, 0.0f);
    float srcTop    = (std::max)(source.y, 0.0f);
    float srcRight  = (std::min)(source.x + source.width, static_cast<float>(texture->width));
    float srcBottom = (std::min)(source.y + source.height, static_cast<float>(texture->height));
    if (srcRight <= srcLeft || srcBottom <= srcTop) return;
    
    D2D1_RECT_F srcRect = D2D1::RectF(srcLeft, srcTop, srcRight, srcBottom);
    D2D1_RECT_F dstRect = to_d2d_rect(destination);
    m_RenderTarget->DrawBitmap(texture->bitmap.Get(), dstRect, m_Alpha,
                               D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
}

bool direct2d_renderer::query_image_size(const std::string& image_path, uint32_t& width, uint32_t& height) {
    texture_resource* texture = load_texture(image_path);
    if (!texture) return false;
    width = texture->width;
    height = texture->height;
    return true;
}


void direct2d_renderer::push_transform(float x, float y, float rotation, float scale_x, float scale_y) {
    m_TransformStack.push_back(m_CurrentTransform);
    D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(x, y);
    D2D1::Matrix3x2F rotationMat = D2D1::Matrix3x2F::Rotation(rotation);
    D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(scale_x, scale_y);
    
    m_CurrentTransform.matrix = translation * rotationMat * scale * m_CurrentTransform.matrix;
    if (m_RenderTarget) {
        m_RenderTarget->SetTransform(m_CurrentTransform.matrix);
    }
}

void direct2d_renderer::pop_transform() {
    if (m_TransformStack.empty()) return;
    m_CurrentTransform = m_TransformStack.back();
    m_TransformStack.pop_back();
    if (m_RenderTarget) {
        m_RenderTarget->SetTransform(m_CurrentTransform.matrix);
    }
}


void direct2d_renderer::set_blend_mode(bool enabled) {
    m_BlendEnabled = enabled;
    
}

void direct2d_renderer::set_alpha(float alpha) {
    m_Alpha = std::clamp(alpha, 0.0f, 1.0f);
}

void direct2d_renderer::get_viewport_size(uint32_t& width, uint32_t& height) const {
    width = m_Width;
    height = m_Height;
}

float direct2d_renderer::measure_text_width(const std::string& text, float font_size,
                                             const std::string& font_family) {
    if (text.empty()) return 0.0f;
    if (!m_DWriteFactory || !m_RenderTarget) return 0.0f;

    measure_key key{text, font_size, -1.0f, font_family};
    auto cit = m_MeasureCache.find(key);
    if (cit != m_MeasureCache.end()) return cit->second;

    font_resource* font = get_font(font_family, font_size);
    if (!font || !font->format) return 0.0f;

    std::wstring wtext = string_to_wstring(text);
    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = m_DWriteFactory->CreateTextLayout(
        wtext.c_str(), static_cast<UINT32>(wtext.length()),
        font->format.Get(), 10000.0f, 100.0f, &layout);
    if (FAILED(hr)) return 0.0f;

    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);
    float result = metrics.widthIncludingTrailingWhitespace;
    if (m_MeasureCache.size() >= MEASURE_CACHE_MAX) m_MeasureCache.clear();
    m_MeasureCache.emplace(std::move(key), result);
    return result;
}

float direct2d_renderer::measure_text_height(const std::string& text, float font_size,
                                              const std::string& font_family,
                                              float wrap_width) {
    if (text.empty()) return 0.0f;
    if (!m_DWriteFactory || !m_RenderTarget) return 0.0f;

    measure_key key{text, font_size, wrap_width, font_family};
    auto cit = m_MeasureCache.find(key);
    if (cit != m_MeasureCache.end()) return cit->second;

    font_resource* font = get_font(font_family, font_size);
    if (!font || !font->format) return 0.0f;

    std::wstring wtext = string_to_wstring(text);
    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = m_DWriteFactory->CreateTextLayout(
        wtext.c_str(), static_cast<UINT32>(wtext.length()),
        font->format.Get(), wrap_width, 10000.0f, &layout);
    if (FAILED(hr)) return 0.0f;

    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);
    float result = metrics.height;
    if (m_MeasureCache.size() >= MEASURE_CACHE_MAX) m_MeasureCache.clear();
    m_MeasureCache.emplace(std::move(key), result);
    return result;
}


float direct2d_renderer::get_dpi() const {
    if (!m_hWnd) return 96.0f;
    HDC hdc = GetDC(m_hWnd);
    float dpiX = static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSX));
    ReleaseDC(m_hWnd, hdc);
    return dpiX;
}

bool direct2d_renderer::create_device_resources() {
    if (!m_hWnd || !m_D2DFactory) return false;

    float dpi = get_dpi();
    UINT pixelWidth = static_cast<UINT>(std::lround(m_Width * dpi / 96.0f));
    UINT pixelHeight = static_cast<UINT>(std::lround(m_Height * dpi / 96.0f));
    D2D1_SIZE_U size = D2D1::SizeU(pixelWidth, pixelHeight);

    
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi, dpi, D2D1_RENDER_TARGET_USAGE_NONE);

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps(m_hWnd, size, D2D1_PRESENT_OPTIONS_NONE);
    HRESULT hr = m_D2DFactory->CreateHwndRenderTarget(rtProps, hwndProps,
        m_RenderTarget.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_RenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
        m_Brush.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { m_RenderTarget.Reset(); return false; }

    
    m_RenderTarget->SetDpi(dpi, dpi);

    
    m_RenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    return true;
}

void direct2d_renderer::release_device_resources() {
    m_Brush.Reset();
    m_RenderTarget.Reset();
    
    m_Textures.clear();
    
    m_Fonts.clear();
    m_MeasureCache.clear();
}


ComPtr<ID2D1Factory> direct2d_renderer::create_d2d_factory() {
    ComPtr<ID2D1Factory> factory;
    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   __uuidof(ID2D1Factory), &options,
                                   reinterpret_cast<void**>(factory.GetAddressOf()));
    return SUCCEEDED(hr) ? factory : nullptr;
}

ComPtr<IDWriteFactory> direct2d_renderer::create_dwrite_factory() {
    ComPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                     __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
    return SUCCEEDED(hr) ? factory : nullptr;
}

ComPtr<IWICImagingFactory> direct2d_renderer::create_wic_factory() {
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    return SUCCEEDED(hr) ? factory : nullptr;
}


direct2d_renderer::texture_resource* direct2d_renderer::load_texture(const std::string& path) {
    auto it = m_Textures.find(path);
    if (it != m_Textures.end()) return &it->second;
    
    if (!m_WICFactory || !m_RenderTarget) return nullptr;
    
    std::wstring wpath = string_to_wstring(path);
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = m_WICFactory->CreateDecoderFromFilename(wpath.c_str(), nullptr,
                                                         GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                         decoder.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    ComPtr<IWICFormatConverter> converter;
    hr = m_WICFactory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0f,
                               WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return nullptr;
    
    UINT width, height;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr)) return nullptr;
    
    ComPtr<ID2D1Bitmap> bitmap;
    hr = m_RenderTarget->CreateBitmapFromWicBitmap(converter.Get(), bitmap.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    texture_resource resource{bitmap, width, height};
    auto result = m_Textures.emplace(path, std::move(resource));
    return &result.first->second;
}


direct2d_renderer::font_resource* direct2d_renderer::get_font(const std::string& family, float size,
                                                              DWRITE_FONT_WEIGHT weight,
                                                              DWRITE_FONT_STYLE style,
                                                              DWRITE_FONT_STRETCH stretch) {
    
    int intSize = static_cast<int>(std::round(size * 100.0f));
    std::string key = family + "_" + std::to_string(intSize) + "_" +
                      std::to_string(static_cast<int>(weight)) + "_" +
                      std::to_string(static_cast<int>(style));
    
    auto it = m_Fonts.find(key);
    if (it != m_Fonts.end()) return &it->second;
    
    if (!m_DWriteFactory) return nullptr;
    
    std::wstring wfamily = string_to_wstring(family.empty() ? "Arial" : family);
    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_DWriteFactory->CreateTextFormat(wfamily.c_str(), nullptr,
                                                   weight, style, stretch, size,
                                                   L"", format.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    font_resource resource{format, size};
    auto result = m_Fonts.emplace(key, std::move(resource));
    return &result.first->second;
}


D2D1_COLOR_F direct2d_renderer::to_d2d_color(const color& c) const {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

D2D1_RECT_F direct2d_renderer::to_d2d_rect(const rectangle& r) const {
    float left = r.x, top = r.y;
    float right = r.x + r.width, bottom = r.y + r.height;
    if (left > right) std::swap(left, right);
    if (top > bottom) std::swap(top, bottom);
    return D2D1::RectF(left, top, right, bottom);
}

D2D1_POINT_2F direct2d_renderer::to_d2d_point(const point& p) const {
    return D2D1::Point2F(p.x, p.y);
}


std::shared_ptr<renderer> renderer::create_direct2d_renderer() {
    return std::make_shared<direct2d_renderer>();
}

} 