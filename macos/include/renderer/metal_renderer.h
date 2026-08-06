/**
 * @file metal_renderer.h
 * @brief Metal 渲染器实现。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace spiration {

/**
 * @brief Metal 2D 渲染器实现。
 */
class metal_renderer : public renderer {
public:
    metal_renderer();
    ~metal_renderer() override;

    metal_renderer(const metal_renderer&) = delete;
    metal_renderer& operator=(const metal_renderer&) = delete;

    metal_renderer(metal_renderer&& other) noexcept;
    metal_renderer& operator=(metal_renderer&& other) noexcept;

    bool initialize(void* native_window_handle) override;
    void shutdown() override;
    void resize(uint32_t width, uint32_t height) override;

    void begin_frame() override;
    void end_frame() override;
    void clear(const color& clear_color) override;

    void draw_rectangle(const rectangle& rect, const color& fill_color) override;
    void draw_rectangle_outline(const rectangle& rect, const color& stroke_color, float stroke_width = 1.0f) override;
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
    bool init_metal();
    bool init_buffers();
    bool init_pipeline_states();
    bool init_sampler_states();
    void release_metal_resources();

    id<MTLDevice> m_Device = nil;
    id<MTLCommandQueue> m_CommandQueue = nil;
    CAMetalLayer* m_MetalLayer = nil;
    id<CAMetalDrawable> m_Drawable = nil;
    id<MTLCommandBuffer> m_CommandBuffer = nil;
    id<MTLRenderCommandEncoder> m_CommandEncoder = nil;

    id<MTLRenderPipelineState> m_BasicPipeline = nil;
    id<MTLRenderPipelineState> m_TexturePipeline = nil;

    id<MTLSamplerState> m_LinearSampler = nil;
    id<MTLSamplerState> m_NearestSampler = nil;

    id<MTLBuffer> m_QuadBuffer = nil;
    id<MTLBuffer> m_TextureQuadBuffer = nil;

    id<MTLTexture> m_CurrentDrawableTexture = nil;

    struct float4x4 {
        float m[16];
    };
    std::vector<float4x4> m_TransformStack;
    std::vector<rectangle> m_ClipStack;
    float4x4 m_CurrentTransform;
    float4x4 m_Projection;

    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    float m_Alpha = 1.0f;
    bool m_BlendEnabled = true;
    bool m_Initialized = false;

    id<MTLTexture> load_image_texture(const std::string& path);
    void draw_text_impl(const std::string& text, const point& position, const color& text_color,
                        float font_size, const std::string& font_family, bool word_wrap);

    struct Vertex2D {
        float x, y;
    };
    struct TextVertex {
        float x, y;
        float u, v;
    };
    struct TextureVertex {
        float x, y;
        float u, v;
    };

    std::unordered_map<std::string, id<MTLTexture>> m_ImageCache;
};

} // namespace spiration
