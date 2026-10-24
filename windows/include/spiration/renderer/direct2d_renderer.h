/**
 * @file direct2d_renderer.h
 * @brief Direct2D 渲染器实现。
 * @author clk
 */

#pragma once

#include <spiration/renderer/renderer.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <unordered_map>
#include <list>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <functional>

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
    /** @brief 重新读取窗口 DPI 并缓存（仅在初始化、尺寸/DPI 变化时调用）。 */
    void refresh_dpi();
    float get_dpi() const;
    /** @brief 按需提交当前变换，避免同一次变换下的连续绘制重复调用 SetTransform。 */
    void commit_transform();

    /** @brief 组合两个哈希值（boost::hash_combine 的等价实现）。 */
    static size_t combine_hash(size_t seed, size_t value) {
        return seed ^ (value + static_cast<size_t>(0x9e3779b9u) + (seed << 6) + (seed >> 2));
    }

    /** @brief 哈希 float 值的二进制表示。 */
    static size_t hash_float(float value) {
        // +0.0f 与 -0.0f 在键比较中相等，哈希必须归一化，否则违反 unordered_map 对相等键的哈希一致性要求。
        if (value == 0.0f) {
            value = 0.0f;
        }
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return static_cast<size_t>(bits);
    }
    
    Microsoft::WRL::ComPtr<ID2D1Factory> create_d2d_factory();
    Microsoft::WRL::ComPtr<IDWriteFactory> create_dwrite_factory();
    Microsoft::WRL::ComPtr<IWICImagingFactory> create_wic_factory();
    
    
    struct texture_resource {
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t last_use = 0;                            ///< 最近使用的帧序号（用于有界淘汰）
    };
    
    struct font_resource {
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        float size = 0.0f;
    };

    /** @brief 字体资源缓存键。
     *
     *  旧实现每次调用 get_font() 都用 std::string 拼接
     *  "family_size_weight_style" 作为键，产生堆分配与字符串比较；
     *  这里改为定长结构体键 + 自定义哈希，命中路径只做整数/字符串比较。
     */
    struct font_key {
        std::string family;
        int size_hundredths = 0;
        int weight = 0;
        int style = 0;
        int stretch = 0;

        bool operator==(const font_key& other) const {
            return size_hundredths == other.size_hundredths &&
                   weight == other.weight &&
                   style == other.style &&
                   stretch == other.stretch &&
                   family == other.family;
        }
    };

    struct font_key_hash {
        size_t operator()(const font_key& key) const {
            size_t hash = std::hash<std::string>()(key.family);
            hash = combine_hash(hash, static_cast<size_t>(key.size_hundredths));
            hash = combine_hash(hash, static_cast<size_t>(key.weight));
            hash = combine_hash(hash, static_cast<size_t>(key.style));
            hash = combine_hash(hash, static_cast<size_t>(key.stretch));
            return hash;
        }
    };
    
    texture_resource* load_texture(const std::string& path);
    /** @brief 纹理缓存达到上限时淘汰最久未使用的纹理（位图依赖 RenderTarget，淘汰即释放）。 */
    void evict_oldest_texture();
    font_resource* get_font(const std::string& family, float size,
                            DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                            DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL,
                            DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL);

    /** @brief 文本布局缓存的换行模式。 */
    enum class text_wrap_mode {
        default_wrap = 0,   ///< 使用 IDWriteTextFormat 的默认换行（自动换行）
        no_wrap = 1         ///< 强制不换行（对应 DWRITE_WORD_WRAPPING_NO_WRAP）
    };

    /** @brief 文本布局缓存键。
     *
     *  布局宽度/高度、水平/垂直对齐、换行模式都会影响 IDWriteTextLayout 的断行与落点，
     *  因此必须全部纳入键；只把与文本内容、字体属性相关的项用于创建布局。
     *  布局只依赖 IDWriteFactory，与渲染目标无关，设备丢失时无需清空。
     */
    struct text_layout_key {
        std::string text;
        std::string font_family;
        float font_size = 0.0f;
        float layout_width = 0.0f;
        float layout_height = 0.0f;
        int h_align = 0;                                  ///< DWRITE_TEXT_ALIGNMENT 的整数值
        int v_align = 0;                                  ///< DWRITE_PARAGRAPH_ALIGNMENT 的整数值
        text_wrap_mode wrap = text_wrap_mode::default_wrap;

        bool operator==(const text_layout_key& other) const {
            return font_size == other.font_size &&
                   layout_width == other.layout_width &&
                   layout_height == other.layout_height &&
                   h_align == other.h_align &&
                   v_align == other.v_align &&
                   wrap == other.wrap &&
                   font_family == other.font_family &&
                   text == other.text;
        }
    };

    struct text_layout_key_hash {
        size_t operator()(const text_layout_key& key) const {
            size_t hash = std::hash<std::string>()(key.text);
            hash = combine_hash(hash, std::hash<std::string>()(key.font_family));
            hash = combine_hash(hash, hash_float(key.font_size));
            hash = combine_hash(hash, hash_float(key.layout_width));
            hash = combine_hash(hash, hash_float(key.layout_height));
            hash = combine_hash(hash, static_cast<size_t>(key.h_align));
            hash = combine_hash(hash, static_cast<size_t>(key.v_align));
            hash = combine_hash(hash, static_cast<size_t>(key.wrap));
            return hash;
        }
    };

    struct text_layout_entry {
        text_layout_key key;                              ///< 供 LRU 淘汰时反查 map 使用
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        size_t estimated_bytes = 0;
    };

    using text_layout_lru = std::list<text_layout_entry>;
    using text_layout_cache = std::unordered_map<text_layout_key, text_layout_lru::iterator, text_layout_key_hash>;

    /** @brief 查找或创建文本布局（命中时提升 LRU 位置），失败返回 nullptr。 */
    IDWriteTextLayout* acquire_text_layout(const text_layout_key& key);
    static size_t estimate_layout_bytes(const text_layout_key& key);
    void evict_text_layouts(size_t incoming_bytes);
    void clear_text_layout_cache();

    /** @brief 文本测量缓存键。 */
    struct measure_key {
        std::string text;
        std::string font_family;
        float font_size = 0.0f;
        float wrap = 0.0f;

        bool operator==(const measure_key& other) const {
            return font_size == other.font_size &&
                   wrap == other.wrap &&
                   font_family == other.font_family &&
                   text == other.text;
        }
    };

    struct measure_key_hash {
        size_t operator()(const measure_key& key) const {
            size_t hash = std::hash<std::string>()(key.text);
            hash = combine_hash(hash, std::hash<std::string>()(key.font_family));
            hash = combine_hash(hash, hash_float(key.font_size));
            hash = combine_hash(hash, hash_float(key.wrap));
            return hash;
        }
    };

    struct measure_entry {
        float value = 0.0f;
        uint64_t last_use = 0;                            ///< 用于有界淘汰时按最近使用时间近似 LRU
    };

    using measure_cache = std::unordered_map<measure_key, measure_entry, measure_key_hash>;

    void evict_measure_cache();
    
    
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
    float m_Dpi = 96.0f;
    uint64_t m_FrameIndex = 0;                            ///< 帧序号，供纹理等资源记录最近使用时间
    
    Microsoft::WRL::ComPtr<ID2D1Factory> m_D2DFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_DWriteFactory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_WICFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_RenderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_Brush;
    
    std::unordered_map<std::string, texture_resource> m_Textures;
    std::unordered_map<font_key, font_resource, font_key_hash> m_Fonts;

    measure_cache m_MeasureCache;
    uint64_t m_CacheClock = 0;                            ///< 测量缓存的逻辑时间戳

    text_layout_cache m_TextLayoutCache;
    text_layout_lru m_TextLayoutLru;                      ///< 头部为最近使用，尾部为最久未使用
    size_t m_TextLayoutCacheBytes = 0;
    
    std::vector<transform> m_TransformStack;
    transform m_CurrentTransform;
    D2D1_MATRIX_3X2_F m_CommittedTransform = D2D1::Matrix3x2F::Identity();
    bool m_TransformCommitted = false;
    
    float m_Alpha = 1.0f;
    bool m_BlendEnabled = true;
    bool m_DeviceLost = false;
    bool m_ComInitialized = false;
    
    static constexpr size_t MEASURE_CACHE_MAX = 16384;
    static constexpr size_t TEXT_LAYOUT_CACHE_MAX = 2048;
    static constexpr size_t TEXT_LAYOUT_CACHE_MAX_BYTES = 4 * 1024 * 1024;
    static constexpr size_t TEXTURE_CACHE_MAX = 256;

    static constexpr wchar_t DEFAULT_FONT[] = L"Arial";
};

} 
