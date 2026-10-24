/**
 * @file direct2d_renderer.cpp
 * @brief Direct2D 渲染器实现。
 * @author clk
 */

#include <spiration/renderer/direct2d_renderer.h>
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
#include <cstring>
#include <utility>
#include <cassert>

#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#endif

using namespace Microsoft::WRL;

namespace spiration {

// C++14 下 static constexpr 数据成员若被 odr-use，需要类外定义；这里统一给出定义，避免潜在链接问题。
constexpr size_t direct2d_renderer::MEASURE_CACHE_MAX;
constexpr size_t direct2d_renderer::TEXT_LAYOUT_CACHE_MAX;
constexpr size_t direct2d_renderer::TEXT_LAYOUT_CACHE_MAX_BYTES;
constexpr size_t direct2d_renderer::TEXTURE_CACHE_MAX;


static std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    if (size_needed == 0) return L"";
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
    return wstr;
}


direct2d_renderer::direct2d_renderer() = default;

direct2d_renderer::~direct2d_renderer() {
    shutdown();
}

direct2d_renderer::direct2d_renderer(direct2d_renderer&& other) noexcept
    : m_hWnd(other.m_hWnd)
    , m_Width(other.m_Width)
    , m_Height(other.m_Height)
    , m_Dpi(other.m_Dpi)
    , m_FrameIndex(other.m_FrameIndex)
    , m_D2DFactory(std::move(other.m_D2DFactory))
    , m_DWriteFactory(std::move(other.m_DWriteFactory))
    , m_WICFactory(std::move(other.m_WICFactory))
    , m_RenderTarget(std::move(other.m_RenderTarget))
    , m_Brush(std::move(other.m_Brush))
    , m_Textures(std::move(other.m_Textures))
    , m_Fonts(std::move(other.m_Fonts))
    , m_MeasureCache(std::move(other.m_MeasureCache))
    , m_CacheClock(other.m_CacheClock)
    , m_TransformStack(std::move(other.m_TransformStack))
    , m_CurrentTransform(other.m_CurrentTransform)
    , m_CommittedTransform(other.m_CommittedTransform)
    , m_TransformCommitted(other.m_TransformCommitted)
    , m_Alpha(other.m_Alpha)
    , m_BlendEnabled(other.m_BlendEnabled)
    , m_DeviceLost(other.m_DeviceLost)
    , m_ComInitialized(other.m_ComInitialized) {
    other.m_TextLayoutCache.clear();
    other.m_TextLayoutLru.clear();
    other.m_TextLayoutCacheBytes = 0;

    other.m_hWnd = nullptr;
    other.m_Width = 0;
    other.m_Height = 0;
    other.m_Dpi = 96.0f;
    other.m_FrameIndex = 0;
    other.m_CacheClock = 0;
    other.m_TransformCommitted = false;
    other.m_Alpha = 1.0f;
    other.m_BlendEnabled = true;
    other.m_DeviceLost = false;
    other.m_ComInitialized = false;
}

direct2d_renderer& direct2d_renderer::operator=(direct2d_renderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        
        m_hWnd = other.m_hWnd;
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_Dpi = other.m_Dpi;
        m_FrameIndex = other.m_FrameIndex;
        m_D2DFactory = std::move(other.m_D2DFactory);
        m_DWriteFactory = std::move(other.m_DWriteFactory);
        m_WICFactory = std::move(other.m_WICFactory);
        m_RenderTarget = std::move(other.m_RenderTarget);
        m_Brush = std::move(other.m_Brush);
        m_Textures = std::move(other.m_Textures);
        m_Fonts = std::move(other.m_Fonts);
        m_MeasureCache = std::move(other.m_MeasureCache);
        m_CacheClock = other.m_CacheClock;
        m_TransformStack = std::move(other.m_TransformStack);
        m_CurrentTransform = other.m_CurrentTransform;
        m_CommittedTransform = other.m_CommittedTransform;
        m_TransformCommitted = other.m_TransformCommitted;
        m_Alpha = other.m_Alpha;
        m_BlendEnabled = other.m_BlendEnabled;
        m_DeviceLost = other.m_DeviceLost;
        m_ComInitialized = other.m_ComInitialized;

        // 目标对象的布局缓存已被 shutdown() 清空；源对象的缓存不转移，直接释放。
        other.m_TextLayoutCache.clear();
        other.m_TextLayoutLru.clear();
        other.m_TextLayoutCacheBytes = 0;
        
        other.m_hWnd = nullptr;
        other.m_Width = 0;
        other.m_Height = 0;
        other.m_Dpi = 96.0f;
        other.m_FrameIndex = 0;
        other.m_CacheClock = 0;
        other.m_TransformCommitted = false;
        other.m_Alpha = 1.0f;
        other.m_BlendEnabled = true;
        other.m_DeviceLost = false;
        other.m_ComInitialized = false;
    }
    return *this;
}

bool direct2d_renderer::initialize(void* native_window_handle) {
    m_hWnd = static_cast<HWND>(native_window_handle);
    if (!m_hWnd || !IsWindow(m_hWnd)) {
        return false;
    }

    // Record whether COM was initialized here so shutdown() can pair CoUninitialize().
    // RPC_E_CHANGED_MODE means another module owns the threads COM - do not uninit it.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == S_OK || hr == S_FALSE) {
        m_ComInitialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    
    m_D2DFactory = create_d2d_factory();
    if (!m_D2DFactory) return false;
    
    m_DWriteFactory = create_dwrite_factory();
    if (!m_DWriteFactory) return false;
    
    m_WICFactory = create_wic_factory();
    if (!m_WICFactory) return false;

    // DPI 只在初始化与 resize（WM_DPICHANGED/WM_SIZE）时读取，之后全部使用缓存值。
    refresh_dpi();

    // 控件树通常有几十层嵌套，预留变换栈容量避免绘制过程中 realloc。
    m_TransformStack.reserve(64);
    
    RECT rc = {};
    GetClientRect(m_hWnd, &rc);
    m_Width = static_cast<uint32_t>((rc.right - rc.left) * 96.0f / m_Dpi);
    m_Height = static_cast<uint32_t>((rc.bottom - rc.top) * 96.0f / m_Dpi);
    
    if (!create_device_resources()) {
        return false;
    }
    
    m_CurrentTransform.matrix = D2D1::Matrix3x2F::Identity();
    return true;
}

void direct2d_renderer::shutdown() {
    release_device_resources();
    clear_text_layout_cache();
    m_TransformStack.clear();
    m_Fonts.clear();
    m_Textures.clear();
    m_MeasureCache.clear();
    m_WICFactory.Reset();
    m_DWriteFactory.Reset();
    m_D2DFactory.Reset();
    m_hWnd = nullptr;
    if (m_ComInitialized) {
        CoUninitialize();
        m_ComInitialized = false;
    }
}

void direct2d_renderer::resize(uint32_t width, uint32_t height) {
    // WM_DPICHANGED 会走这里：即使逻辑尺寸不变，DPI 也可能变化，因此先重读并缓存 DPI。
    const float previous_dpi = m_Dpi;
    refresh_dpi();
    const bool dpi_changed = (previous_dpi != m_Dpi);

    if (m_Width == width && m_Height == height && !dpi_changed) return;
    
    m_Width = width;
    m_Height = height;
    
    if (m_RenderTarget) {
        UINT pw = static_cast<UINT>(std::lround(width * m_Dpi / 96.0f));
        UINT ph = static_cast<UINT>(std::lround(height * m_Dpi / 96.0f));
        HRESULT hr = m_RenderTarget->Resize(D2D1::SizeU(pw, ph));
        if (hr == D2DERR_RECREATE_TARGET) {
            // Target lost (e.g. device removed): recreate on next begin_frame().
            m_DeviceLost = true;
            release_device_resources();
        } else {
            m_RenderTarget->SetDpi(m_Dpi, m_Dpi);
            // 保守处理：不依赖 Resize/SetDpi 保留变换状态，下一帧首次绘制会重新提交。
            m_TransformCommitted = false;
        }
    }
}


void direct2d_renderer::begin_frame() {
    // 帧序号用于纹理等“按最近使用”淘汰的资源，单调递增即可。
    ++m_FrameIndex;
    if (m_DeviceLost) {
        if (!create_device_resources()) {
            return;
        }
        m_DeviceLost = false;
    }
    if (m_RenderTarget) {
        m_RenderTarget->BeginDraw();
        // 变换不再每帧无条件提交：绘制时由 commit_transform() 按需调用 SetTransform，
        // 且只有矩阵真正变化时才提交（同变换下连续绘制 0 次状态切换）。
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
        // Clear() 不受变换影响，无需提交变换。
        m_RenderTarget->Clear(to_d2d_color(clear_color));
    }
}


void direct2d_renderer::draw_rectangle(const rectangle& rectangle, const color& fill_color) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(fill_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    m_RenderTarget->FillRectangle(to_d2d_rect(rectangle), m_Brush.Get());
}

void direct2d_renderer::draw_rectangle_outline(const rectangle& rectangle, const color& stroke_color, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    m_RenderTarget->DrawRectangle(to_d2d_rect(rectangle), m_Brush.Get(), stroke_width);
}

void direct2d_renderer::draw_rounded_rectangle(const rectangle& rect, const color& fill_color, float radius) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(fill_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(to_d2d_rect(rect), radius, radius);
    m_RenderTarget->FillRoundedRectangle(rr, m_Brush.Get());
}

void direct2d_renderer::draw_rounded_rectangle_outline(const rectangle& rect, const color& stroke_color, float radius, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(to_d2d_rect(rect), radius, radius);
    m_RenderTarget->DrawRoundedRectangle(rr, m_Brush.Get(), stroke_width);
}

void direct2d_renderer::push_clip(const rectangle& rect) {
    if (!m_RenderTarget) return;
    // 与旧实现保持一致：压栈时渲染目标已经处于当前变换下。
    commit_transform();
    m_RenderTarget->PushAxisAlignedClip(to_d2d_rect(rect), D2D1_ANTIALIAS_MODE_ALIASED);
}

void direct2d_renderer::pop_clip() {
    if (!m_RenderTarget) return;
    m_RenderTarget->PopAxisAlignedClip();
}

void direct2d_renderer::draw_circle(const point& center, float radius, const color& fill_color) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(fill_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(to_d2d_point(center), radius, radius);
    m_RenderTarget->FillEllipse(ellipse, m_Brush.Get());
}

void direct2d_renderer::draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(to_d2d_point(center), radius, radius);
    m_RenderTarget->DrawEllipse(ellipse, m_Brush.Get(), stroke_width);
}

void direct2d_renderer::draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width) {
    if (!m_RenderTarget || !m_Brush) return;
    commit_transform();
    D2D1_COLOR_F color = to_d2d_color(stroke_color);
    color.a *= m_Alpha;
    m_Brush->SetColor(color);
    m_RenderTarget->DrawLine(to_d2d_point(start), to_d2d_point(end), m_Brush.Get(), stroke_width);
}


void direct2d_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                                  float font_size, const std::string& font_family,
                                  bool word_wrap) {
    if (!m_RenderTarget || !m_DWriteFactory) return;
    if (text.empty()) return;

    text_layout_key key;
    key.text = text;
    key.font_family = font_family.empty() ? "Arial" : font_family;
    key.font_size = font_size;
    // 与旧实现一致：DrawTextW 使用 TextFormat 的默认对齐（leading / near）。
    key.h_align = static_cast<int>(DWRITE_TEXT_ALIGNMENT_LEADING);
    key.v_align = static_cast<int>(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (word_wrap) {
        // 自动换行：布局尺寸取旧实现 DrawTextW 使用的矩形（position 到视口右下角）。
        key.layout_width = static_cast<float>(m_Width);
        key.layout_height = static_cast<float>(m_Height);
    } else {
        // 不换行：旧实现固定用 10000x100 创建布局后再关闭自动换行。
        key.layout_width = 10000.0f;
        key.layout_height = 100.0f;
        key.wrap = text_wrap_mode::no_wrap;
    }

    IDWriteTextLayout* layout = acquire_text_layout(key);
    if (!layout) return;

    D2D1_COLOR_F color = to_d2d_color(text_color);
    color.a *= m_Alpha;
    if (!m_Brush) return;
    m_Brush->SetColor(color);

    commit_transform();
    m_RenderTarget->DrawTextLayout(D2D1::Point2F(position.x, position.y), layout, m_Brush.Get());
}

void direct2d_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                                          text_alignment h_align, vertical_alignment v_align,
                                          float font_size, const std::string& font_family) {
    if (!m_RenderTarget || !m_DWriteFactory) return;
    // 旧实现中 0 尺寸的布局会创建失败（CreateTextLayout 返回 E_INVALIDARG），这里提前返回，绘制结果一致。
    if (text.empty() || bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    
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

    text_layout_key key;
    key.text = text;
    key.font_family = font_family.empty() ? "Arial" : font_family;
    key.font_size = font_size;
    key.layout_width = bounds.width;
    key.layout_height = bounds.height;
    key.h_align = static_cast<int>(dwriteHA);
    key.v_align = static_cast<int>(dwriteVA);

    IDWriteTextLayout* layout = acquire_text_layout(key);
    if (!layout) return;
    
    D2D1_COLOR_F color = to_d2d_color(text_color);
    color.a *= m_Alpha;
    if (!m_Brush) return;
    m_Brush->SetColor(color);
    
    D2D1_POINT_2F origin = D2D1::Point2F(bounds.x, bounds.y);
    commit_transform();
    m_RenderTarget->DrawTextLayout(origin, layout, m_Brush.Get());
}


void direct2d_renderer::draw_image(const std::string& image_path, const rectangle& destination) {
    if (!m_RenderTarget || !m_WICFactory) return;
    texture_resource* texture = load_texture(image_path);
    if (!texture || !texture->bitmap) return;
    commit_transform();
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
    commit_transform();
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
    
    // 只在 CPU 侧累积矩阵；实际 SetTransform 延迟到下一次真正绘制前（commit_transform）。
    m_CurrentTransform.matrix = translation * rotationMat * scale * m_CurrentTransform.matrix;
}

void direct2d_renderer::pop_transform() {
    if (m_TransformStack.empty()) return;
    m_CurrentTransform = m_TransformStack.back();
    m_TransformStack.pop_back();
    // 同样延迟提交：若紧接着没有绘制，则完全省掉一次 SetTransform。
}


void direct2d_renderer::set_blend_mode(bool enabled) {
    m_BlendEnabled = enabled;
    
}

void direct2d_renderer::set_alpha(float alpha) {
    m_Alpha = std::max(0.0f, std::min(1.0f, alpha));
}

void direct2d_renderer::get_viewport_size(uint32_t& width, uint32_t& height) const {
    width = m_Width;
    height = m_Height;
}

float direct2d_renderer::measure_text_width(const std::string& text, float font_size,
                                             const std::string& font_family) {
    if (text.empty()) return 0.0f;
    if (!m_DWriteFactory || !m_RenderTarget) return 0.0f;

    const std::string family = font_family.empty() ? "Arial" : font_family;
    measure_key key{text, family, font_size, -1.0f};
    auto cit = m_MeasureCache.find(key);
    if (cit != m_MeasureCache.end()) {
        cit->second.last_use = ++m_CacheClock;
        return cit->second.value;
    }

    font_resource* font = get_font(family, font_size);
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
    evict_measure_cache();
    measure_entry entry;
    entry.value = result;
    entry.last_use = ++m_CacheClock;
    m_MeasureCache.emplace(std::move(key), entry);
    return result;
}

float direct2d_renderer::measure_text_height(const std::string& text, float font_size,
                                              const std::string& font_family,
                                              float wrap_width) {
    if (text.empty()) return 0.0f;
    if (!m_DWriteFactory || !m_RenderTarget) return 0.0f;

    const std::string family = font_family.empty() ? "Arial" : font_family;
    measure_key key{text, family, font_size, wrap_width};
    auto cit = m_MeasureCache.find(key);
    if (cit != m_MeasureCache.end()) {
        cit->second.last_use = ++m_CacheClock;
        return cit->second.value;
    }

    font_resource* font = get_font(family, font_size);
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
    evict_measure_cache();
    measure_entry entry;
    entry.value = result;
    entry.last_use = ++m_CacheClock;
    m_MeasureCache.emplace(std::move(key), entry);
    return result;
}

void direct2d_renderer::evict_measure_cache() {
    if (m_MeasureCache.size() < MEASURE_CACHE_MAX) return;

    // 有界淘汰：按最近使用时间戳挑出最旧的 1/4 条目删除。
    // 旧实现达到上限后整体 clear()，导致后续帧所有文本重新测量/塑形（卡顿抖动），这里避免全清。
    std::vector<std::pair<uint64_t, measure_cache::iterator>> entries;
    entries.reserve(m_MeasureCache.size());
    for (measure_cache::iterator it = m_MeasureCache.begin(); it != m_MeasureCache.end(); ++it) {
        entries.push_back(std::make_pair(it->second.last_use, it));
    }

    const size_t evict_count = entries.size() / 4 + 1;
    std::nth_element(entries.begin(), entries.begin() + evict_count, entries.end(),
        [](const std::pair<uint64_t, measure_cache::iterator>& lhs,
           const std::pair<uint64_t, measure_cache::iterator>& rhs) {
            return lhs.first < rhs.first;
        });

    for (size_t i = 0; i < evict_count; ++i) {
        m_MeasureCache.erase(entries[i].second);
    }
}


void direct2d_renderer::refresh_dpi() {
    if (!m_hWnd) {
        m_Dpi = 96.0f;
        return;
    }
    HDC hdc = GetDC(m_hWnd);
    if (!hdc) return;
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(m_hWnd, hdc);
    if (dpi > 0) {
        m_Dpi = static_cast<float>(dpi);
    }
}

float direct2d_renderer::get_dpi() const {
    // DPI 在 initialize()/resize() 中缓存，避免每次 get_dpi() 都 GetDC/GetDeviceCaps/ReleaseDC。
    return m_Dpi;
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

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{ m_hWnd, size, D2D1_PRESENT_OPTIONS_NONE };
    HRESULT hr = m_D2DFactory->CreateHwndRenderTarget(rtProps, hwndProps,
        m_RenderTarget.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_RenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
        m_Brush.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { m_RenderTarget.Reset(); return false; }

    
    m_RenderTarget->SetDpi(dpi, dpi);

    
    m_RenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    // 新渲染目标的变换为单位矩阵，标记为“未提交”，首次绘制前会按需提交 m_CurrentTransform。
    m_TransformCommitted = false;
    return true;
}

void direct2d_renderer::release_device_resources() {
    m_Brush.Reset();
    m_RenderTarget.Reset();
    m_TransformCommitted = false;
    
    // IWIC 纹理位图依赖 RenderTarget，必须随设备资源一起释放。
    m_Textures.clear();

    // 注意：m_Fonts（IDWriteTextFormat）与 m_MeasureCache 只依赖 DWrite 工厂，与设备无关。
    // 设备丢失（设备移除、远程桌面切换、驱动更新）是卡顿高发场景，这里保留它们可避免
    // 重建后整屏文本重新 CreateTextFormat + 重新测量；只在 shutdown() 中清理。
    // m_TextLayoutCache 同理由 IDWriteFactory 创建，也在设备丢失时保留。
}

void direct2d_renderer::commit_transform() {
    if (!m_RenderTarget) return;
    if (m_TransformCommitted &&
        std::memcmp(&m_CommittedTransform, &m_CurrentTransform.matrix, sizeof(D2D1_MATRIX_3X2_F)) == 0) {
        return;
    }
    m_RenderTarget->SetTransform(m_CurrentTransform.matrix);
    m_CommittedTransform = m_CurrentTransform.matrix;
    m_TransformCommitted = true;
}


IDWriteTextLayout* direct2d_renderer::acquire_text_layout(const text_layout_key& key) {
    text_layout_cache::iterator cached = m_TextLayoutCache.find(key);
    if (cached != m_TextLayoutCache.end()) {
        // 命中：把条目提升到 LRU 链表头部；splice 不改变节点地址，map 中保存的迭代器仍然有效。
        if (cached->second != m_TextLayoutLru.begin()) {
            m_TextLayoutLru.splice(m_TextLayoutLru.begin(), m_TextLayoutLru, cached->second);
        }
        return cached->second->layout.Get();
    }

    if (!m_DWriteFactory) return nullptr;

    font_resource* font = get_font(key.font_family, key.font_size);
    if (!font || !font->format) return nullptr;

    // 只有未命中时才做 UTF-8 -> UTF-16 转换，命中路径不产生任何临时宽字符串分配。
    std::wstring wtext = string_to_wstring(key.text);
    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = m_DWriteFactory->CreateTextLayout(
        wtext.c_str(), static_cast<UINT32>(wtext.length()), font->format.Get(),
        key.layout_width, key.layout_height, &layout);
    if (FAILED(hr) || !layout) return nullptr;

    // 对齐/换行只在创建时设置一次，随布局一起缓存。
    layout->SetTextAlignment(static_cast<DWRITE_TEXT_ALIGNMENT>(key.h_align));
    layout->SetParagraphAlignment(static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(key.v_align));
    if (key.wrap == text_wrap_mode::no_wrap) {
        layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    const size_t estimated = estimate_layout_bytes(key);
    evict_text_layouts(estimated);

    text_layout_entry entry;
    entry.key = key;
    entry.layout = layout;
    entry.estimated_bytes = estimated;
    m_TextLayoutLru.push_front(std::move(entry));

    std::pair<text_layout_cache::iterator, bool> inserted;
    try {
        inserted = m_TextLayoutCache.emplace(key, m_TextLayoutLru.begin());
    } catch (...) {
        // 极端内存不足时回滚已插入的 LRU 节点，保证 map/list/字节计数三者一致。
        m_TextLayoutLru.pop_front();
        throw;
    }
    if (!inserted.second) {
        // 单线程下不应出现重复插入；万一出现，丢弃刚插入的链表节点并复用已有条目，保证 map/list 一致。
        m_TextLayoutLru.pop_front();
        m_TextLayoutLru.splice(m_TextLayoutLru.begin(), m_TextLayoutLru, inserted.first->second);
        return inserted.first->second->layout.Get();
    }
    m_TextLayoutCacheBytes += estimated;
    return inserted.first->second->layout.Get();
}

size_t direct2d_renderer::estimate_layout_bytes(const text_layout_key& key) {
    // 粗略估算布局占用：DWrite 内部以 UTF-16 保存文本，另含塑形/断行数据。
    return (key.text.size() + key.font_family.size()) * sizeof(wchar_t) + 256;
}

void direct2d_renderer::evict_text_layouts(size_t incoming_bytes) {
    // 有界淘汰：从 LRU 链表尾部（最久未使用）释放，直到条目数和估算字节数都满足上限。
    // 与旧的“测量缓存整体 clear()”不同，这里逐条淘汰，热文本布局始终保留。
    while (!m_TextLayoutLru.empty() &&
           (m_TextLayoutLru.size() >= TEXT_LAYOUT_CACHE_MAX ||
            m_TextLayoutCacheBytes + incoming_bytes > TEXT_LAYOUT_CACHE_MAX_BYTES)) {
        const text_layout_entry& victim = m_TextLayoutLru.back();
        m_TextLayoutCache.erase(victim.key);
        m_TextLayoutCacheBytes -= victim.estimated_bytes;
        m_TextLayoutLru.pop_back();
    }
}

void direct2d_renderer::clear_text_layout_cache() {
    m_TextLayoutCache.clear();
    m_TextLayoutLru.clear();
    m_TextLayoutCacheBytes = 0;
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
    if (it != m_Textures.end()) {
        it->second.last_use = m_FrameIndex;
        return &it->second;
    }
    
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

    // 有界缓存：先按 LRU 淘汰再插入，避免图片多的界面纹理内存无界增长。
    evict_oldest_texture();
    
    texture_resource resource{bitmap, width, height, m_FrameIndex};
    auto result = m_Textures.emplace(path, std::move(resource));
    return &result.first->second;
}

void direct2d_renderer::evict_oldest_texture() {
    if (m_Textures.size() < TEXTURE_CACHE_MAX) return;

    // 条目数很少（上限 256），线性扫描找最久未使用的条目即可，仅在加载新图片时触发。
    auto oldest = m_Textures.begin();
    for (auto it = m_Textures.begin(); it != m_Textures.end(); ++it) {
        if (it->second.last_use < oldest->second.last_use) {
            oldest = it;
        }
    }

    // ID2D1Bitmap 依赖 RenderTarget，淘汰即释放；调用点只在 load_texture() 中、
    // 返回的指针仅在当次 draw_image/query_image_size 内使用，不会淘汰正在使用的纹理。
    m_Textures.erase(oldest);
}


direct2d_renderer::font_resource* direct2d_renderer::get_font(const std::string& family, float size,
                                                              DWRITE_FONT_WEIGHT weight,
                                                              DWRITE_FONT_STYLE style,
                                                              DWRITE_FONT_STRETCH stretch) {
    // 结构体键替代原来的字符串拼接键（family + "_" + size + "_" + weight + ...），
    // 命中路径不再有堆分配与多次字符串比较。顺带把原先漏掉的 stretch 纳入键。
    font_key key;
    key.family = family.empty() ? "Arial" : family;
    key.size_hundredths = static_cast<int>(std::round(size * 100.0f));
    key.weight = static_cast<int>(weight);
    key.style = static_cast<int>(style);
    key.stretch = static_cast<int>(stretch);
    
    auto it = m_Fonts.find(key);
    if (it != m_Fonts.end()) return &it->second;
    
    if (!m_DWriteFactory) return nullptr;
    
    std::wstring wfamily = string_to_wstring(key.family);
    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_DWriteFactory->CreateTextFormat(wfamily.c_str(), nullptr,
                                                   weight, style, stretch, size,
                                                   L"", format.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    font_resource resource{format, size};
    auto result = m_Fonts.emplace(std::move(key), std::move(resource));
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
