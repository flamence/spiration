/**
 * @file metal_renderer.mm
 * @brief Metal 2D 渲染器实现。
 * @author clk
 */

#import <renderer/metal_renderer.h>
#import <ui/color.h>
#import <ui/point.h>
#import <ui/rectangle.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreText/CoreText.h>
#import <CoreImage/CoreImage.h>
#import <simd/simd.h>
#import <cstring>
#import <cmath>
#import <algorithm>
#import <string>
#import <vector>

namespace spiration {

static matrix_float4x4 mat4_identity() {
    return matrix_identity_float4x4;
}

static matrix_float4x4 mat4_ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
    matrix_float4x4 m = {};
    m.columns[0] = (simd_float4){ 2.0f / (right - left), 0, 0, 0 };
    m.columns[1] = (simd_float4){ 0, 2.0f / (top - bottom), 0, 0 };
    m.columns[2] = (simd_float4){ 0, 0, -2.0f / (farZ - nearZ), 0 };
    m.columns[3] = (simd_float4){ -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(farZ + nearZ) / (farZ - nearZ), 1 };
    return m;
}

static matrix_float4x4 mat4_translate(matrix_float4x4 m, float x, float y) {
    matrix_float4x4 t = mat4_identity();
    t.columns[3] = (simd_float4){ x, y, 0, 1 };
    return matrix_multiply(m, t);
}

static matrix_float4x4 mat4_scale(matrix_float4x4 m, float sx, float sy) {
    matrix_float4x4 s = mat4_identity();
    s.columns[0].x = sx;
    s.columns[1].y = sy;
    return matrix_multiply(m, s);
}

metal_renderer::metal_renderer() {
    m_Projection = {};
    m_CurrentTransform = {};
    m_CurrentTransform.m[0] = 1.0f;
    m_CurrentTransform.m[5] = 1.0f;
    m_CurrentTransform.m[10] = 1.0f;
    m_CurrentTransform.m[15] = 1.0f;
}

metal_renderer::~metal_renderer() {
    shutdown();
}

metal_renderer::metal_renderer(metal_renderer&& other) noexcept
    : m_Device(other.m_Device)
    , m_CommandQueue(other.m_CommandQueue)
    , m_MetalLayer(other.m_MetalLayer)
    , m_Drawable(other.m_Drawable)
    , m_CommandBuffer(other.m_CommandBuffer)
    , m_CommandEncoder(other.m_CommandEncoder)
    , m_BasicPipeline(other.m_BasicPipeline)
    , m_TextPipeline(other.m_TextPipeline)
    , m_TexturePipeline(other.m_TexturePipeline)
    , m_LinearSampler(other.m_LinearSampler)
    , m_NearestSampler(other.m_NearestSampler)
    , m_QuadBuffer(other.m_QuadBuffer)
    , m_CircleBuffer(other.m_CircleBuffer)
    , m_DepthState(other.m_DepthState)
    , m_CurrentDrawableTexture(other.m_CurrentDrawableTexture)
    , m_TransformStack(std::move(other.m_TransformStack))
    , m_CurrentTransform(other.m_CurrentTransform)
    , m_Width(other.m_Width)
    , m_Height(other.m_Height)
    , m_Alpha(other.m_Alpha)
    , m_BlendEnabled(other.m_BlendEnabled)
    , m_Initialized(other.m_Initialized)
{
    std::memcpy(&m_Projection, &other.m_Projection, sizeof(m_Projection));
    other.m_Device = nil;
    other.m_CommandQueue = nil;
    other.m_MetalLayer = nil;
    other.m_Drawable = nil;
    other.m_CommandBuffer = nil;
    other.m_CommandEncoder = nil;
    other.m_BasicPipeline = nil;
    other.m_TextPipeline = nil;
    other.m_TexturePipeline = nil;
    other.m_LinearSampler = nil;
    other.m_NearestSampler = nil;
    other.m_QuadBuffer = nil;
    other.m_CircleBuffer = nil;
    other.m_DepthState = nil;
    other.m_CurrentDrawableTexture = nil;
    other.m_Width = 0;
    other.m_Height = 0;
    other.m_Alpha = 1.0f;
    other.m_BlendEnabled = true;
    other.m_Initialized = false;
}

metal_renderer& metal_renderer::operator=(metal_renderer&& other) noexcept {
    if (this != &other) {
        shutdown();

        m_Device = other.m_Device;
        m_CommandQueue = other.m_CommandQueue;
        m_MetalLayer = other.m_MetalLayer;
        m_Drawable = other.m_Drawable;
        m_CommandBuffer = other.m_CommandBuffer;
        m_CommandEncoder = other.m_CommandEncoder;
        m_BasicPipeline = other.m_BasicPipeline;
        m_TextPipeline = other.m_TextPipeline;
        m_TexturePipeline = other.m_TexturePipeline;
        m_LinearSampler = other.m_LinearSampler;
        m_NearestSampler = other.m_NearestSampler;
        m_QuadBuffer = other.m_QuadBuffer;
        m_CircleBuffer = other.m_CircleBuffer;
        m_DepthState = other.m_DepthState;
        m_CurrentDrawableTexture = other.m_CurrentDrawableTexture;
        m_TransformStack = std::move(other.m_TransformStack);
        m_CurrentTransform = other.m_CurrentTransform;
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_Alpha = other.m_Alpha;
        m_BlendEnabled = other.m_BlendEnabled;
        m_Initialized = other.m_Initialized;
        std::memcpy(&m_Projection, &other.m_Projection, sizeof(m_Projection));

        other.m_Device = nil;
        other.m_CommandQueue = nil;
        other.m_MetalLayer = nil;
        other.m_Drawable = nil;
        other.m_CommandBuffer = nil;
        other.m_CommandEncoder = nil;
        other.m_BasicPipeline = nil;
        other.m_TextPipeline = nil;
        other.m_TexturePipeline = nil;
        other.m_LinearSampler = nil;
        other.m_NearestSampler = nil;
        other.m_QuadBuffer = nil;
        other.m_CircleBuffer = nil;
        other.m_DepthState = nil;
        other.m_CurrentDrawableTexture = nil;
        other.m_Width = 0;
        other.m_Height = 0;
        other.m_Alpha = 1.0f;
        other.m_BlendEnabled = true;
        other.m_Initialized = false;
    }
    return *this;
}

bool metal_renderer::initialize(void* native_window_handle) {
    m_MetalLayer = (__bridge CAMetalLayer*)native_window_handle;
    if (!m_MetalLayer) return false;

    if (!init_metal()) return false;
    if (!init_buffers()) return false;
    if (!init_pipeline_states()) return false;
    if (!init_sampler_states()) return false;

    m_Projection = {};
    @autoreleasepool {
        CGSize drawableSize = m_MetalLayer.drawableSize;
        m_Width = static_cast<uint32_t>(drawableSize.width);
        m_Height = static_cast<uint32_t>(drawableSize.height);
    }

    matrix_float4x4 proj = mat4_ortho(0.0f, static_cast<float>(m_Width),
                                       static_cast<float>(m_Height), 0.0f, -1.0f, 1.0f);
    std::memcpy(m_Projection.m, &proj, sizeof(m_Projection));

    m_CurrentTransform = {};
    m_CurrentTransform.m[0] = 1.0f;
    m_CurrentTransform.m[5] = 1.0f;
    m_CurrentTransform.m[10] = 1.0f;
    m_CurrentTransform.m[15] = 1.0f;

    m_Initialized = true;
    return true;
}

void metal_renderer::shutdown() {
    if (!m_Initialized) return;
    release_metal_resources();
    m_Initialized = false;
}

void metal_renderer::resize(uint32_t width, uint32_t height) {
    if (m_Width == width && m_Height == height) return;
    m_Width = width;
    m_Height = height;

    @autoreleasepool {
        m_MetalLayer.drawableSize = CGSizeMake(width, height);
    }

    matrix_float4x4 proj = mat4_ortho(0.0f, static_cast<float>(width),
                                       static_cast<float>(height), 0.0f, -1.0f, 1.0f);
    std::memcpy(m_Projection.m, &proj, sizeof(m_Projection));
}

void metal_renderer::begin_frame() {
    @autoreleasepool {
        m_Drawable = [m_MetalLayer nextDrawable];
        if (!m_Drawable) return;

        m_CurrentDrawableTexture = [m_Drawable texture];
        m_CommandBuffer = [m_CommandQueue commandBuffer];

        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = m_CurrentDrawableTexture;
        desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

        m_CommandEncoder = [m_CommandBuffer renderCommandEncoderWithDescriptor:desc];
        [m_CommandEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
        [m_CommandEncoder setCullMode:MTLCullModeNone];
    }
}

void metal_renderer::end_frame() {
    @autoreleasepool {
        [m_CommandEncoder endEncoding];
        m_CommandEncoder = nil;

        [m_CommandBuffer presentDrawable:m_Drawable];
        [m_CommandBuffer commit];
        m_CommandBuffer = nil;
        m_Drawable = nil;
        m_CurrentDrawableTexture = nil;
    }
}

void metal_renderer::clear(const color& clear_color) {
    @autoreleasepool {
        if (m_CommandEncoder) {
            [m_CommandEncoder endEncoding];
            m_CommandEncoder = nil;
        }

        m_Drawable = [m_MetalLayer nextDrawable];
        if (!m_Drawable) return;

        m_CurrentDrawableTexture = [m_Drawable texture];
        m_CommandBuffer = [m_CommandQueue commandBuffer];

        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = m_CurrentDrawableTexture;
        desc.colorAttachments[0].loadAction = MTLLoadActionClear;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = MTLClearColorMake(clear_color.r, clear_color.g, clear_color.b, clear_color.a);

        m_CommandEncoder = [m_CommandBuffer renderCommandEncoderWithDescriptor:desc];
    }
}

void metal_renderer::draw_rectangle(const rectangle& rect, const color& fill_color) {
    if (!m_CommandEncoder || !m_BasicPipeline) return;

    @autoreleasepool {
        [m_CommandEncoder setRenderPipelineState:m_BasicPipeline];

        matrix_float4x4 model = mat4_identity();
        model = mat4_translate(model, rect.x, rect.y);
        model = mat4_scale(model, rect.width, rect.height);

        matrix_float4x4 view = {};
        std::memcpy(&view, m_CurrentTransform.m, sizeof(view));

        matrix_float4x4 proj = {};
        std::memcpy(&proj, m_Projection.m, sizeof(proj));

        matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

        struct Uniforms {
            matrix_float4x4 mvp;
            simd_float4 color;
        };
        Uniforms uniforms = { mvp, { fill_color.r, fill_color.g, fill_color.b, fill_color.a * m_Alpha } };

        id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                             length:sizeof(Uniforms)
                                                            options:MTLResourceStorageModeShared];

        [m_CommandEncoder setVertexBuffer:m_QuadBuffer offset:0 atIndex:0];
        [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
        [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:0];
        [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    }
}

void metal_renderer::draw_rectangle_outline(const rectangle& rect, const color& stroke_color, float stroke_width) {
    float t = stroke_width;
    draw_rectangle({ rect.x, rect.y, rect.width, t }, stroke_color);
    draw_rectangle({ rect.x, rect.y + rect.height - t, rect.width, t }, stroke_color);
    draw_rectangle({ rect.x, rect.y + t, t, rect.height - 2.0f * t }, stroke_color);
    draw_rectangle({ rect.x + rect.width - t, rect.y + t, t, rect.height - 2.0f * t }, stroke_color);
}

void metal_renderer::draw_circle(const point& center, float radius, const color& fill_color) {
    if (!m_CommandEncoder || !m_BasicPipeline) return;

    const int segments = 64;
    struct CircleVertex {
        float x, y;
    };
    std::vector<CircleVertex> verts;
    verts.push_back({ 0.0f, 0.0f });
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(segments);
        verts.push_back({ cosf(angle), sinf(angle) });
    }

    @autoreleasepool {
        id<MTLBuffer> vertBuffer = [m_Device newBufferWithBytes:verts.data()
                                                          length:verts.size() * sizeof(CircleVertex)
                                                         options:MTLResourceStorageModeShared];

        [m_CommandEncoder setRenderPipelineState:m_BasicPipeline];

        matrix_float4x4 model = mat4_identity();
        model = mat4_translate(model, center.x, center.y);
        model = mat4_scale(model, radius, radius);

        matrix_float4x4 view = {};
        std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
        matrix_float4x4 proj = {};
        std::memcpy(&proj, m_Projection.m, sizeof(proj));
        matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

        struct Uniforms {
            matrix_float4x4 mvp;
            simd_float4 color;
        };
        Uniforms uniforms = { mvp, { fill_color.r, fill_color.g, fill_color.b, fill_color.a * m_Alpha } };
        id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                             length:sizeof(Uniforms)
                                                            options:MTLResourceStorageModeShared];

        [m_CommandEncoder setVertexBuffer:vertBuffer offset:0 atIndex:0];
        [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
        [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:0];
        [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleFan vertexStart:0 vertexCount:verts.size()];
    }
}

void metal_renderer::draw_circle_outline(const point& center, float radius, const color& stroke_color, float stroke_width) {
    draw_circle(center, radius, stroke_color);

    const int segments = 48;
    struct LineVertex {
        float x, y;
    };
    std::vector<LineVertex> verts;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(segments);
        verts.push_back({ radius * cosf(angle), radius * sinf(angle) });
    }

    @autoreleasepool {
        id<MTLBuffer> vertBuffer = [m_Device newBufferWithBytes:verts.data()
                                                          length:verts.size() * sizeof(LineVertex)
                                                         options:MTLResourceStorageModeShared];

        [m_CommandEncoder setRenderPipelineState:m_BasicPipeline];

        matrix_float4x4 model = mat4_identity();
        model = mat4_translate(model, center.x, center.y);

        matrix_float4x4 view = {};
        std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
        matrix_float4x4 proj = {};
        std::memcpy(&proj, m_Projection.m, sizeof(proj));
        matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

        struct Uniforms {
            matrix_float4x4 mvp;
            simd_float4 color;
        };
        Uniforms uniforms = { mvp, { stroke_color.r, stroke_color.g, stroke_color.b, stroke_color.a * m_Alpha } };
        id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                             length:sizeof(Uniforms)
                                                            options:MTLResourceStorageModeShared];

        [m_CommandEncoder setVertexBuffer:vertBuffer offset:0 atIndex:0];
        [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
        [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:0];
        [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeLineStrip vertexStart:0 vertexCount:verts.size()];
    }
}

void metal_renderer::draw_line(const point& start, const point& end, const color& stroke_color, float stroke_width) {
    if (!m_CommandEncoder || !m_BasicPipeline) return;

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    @autoreleasepool {
        [m_CommandEncoder setRenderPipelineState:m_BasicPipeline];

        matrix_float4x4 model = mat4_identity();
        model = mat4_translate(model, start.x, start.y);
        float angle = atan2f(dy, dx) * 180.0f / M_PI;
        float rad = atan2f(dy, dx);
        matrix_float4x4 rot = mat4_identity();
        rot.columns[0] = (simd_float4){ cosf(rad), sinf(rad), 0, 0 };
        rot.columns[1] = (simd_float4){ -sinf(rad), cosf(rad), 0, 0 };
        model = matrix_multiply(model, rot);
        model = mat4_scale(model, len, stroke_width);

        matrix_float4x4 view = {};
        std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
        matrix_float4x4 proj = {};
        std::memcpy(&proj, m_Projection.m, sizeof(proj));
        matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

        struct Uniforms {
            matrix_float4x4 mvp;
            simd_float4 color;
        };
        Uniforms uniforms = { mvp, { stroke_color.r, stroke_color.g, stroke_color.b, stroke_color.a * m_Alpha } };
        id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                             length:sizeof(Uniforms)
                                                            options:MTLResourceStorageModeShared];

        [m_CommandEncoder setVertexBuffer:m_QuadBuffer offset:0 atIndex:0];
        [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
        [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:0];
        [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    }
}

void metal_renderer::draw_text(const std::string& text, const point& position, const color& text_color,
                               float font_size, const std::string& font_family,
                               bool word_wrap) {
    if (text.empty()) return;

    @autoreleasepool {
        NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
        NSString* nsFontFamily = font_family.empty()
            ? @"Arial" : [NSString stringWithUTF8String:font_family.c_str()];

        NSFont* font = [NSFont fontWithName:nsFontFamily size:font_size];
        if (!font) {
            font = [NSFont systemFontOfSize:font_size];
        }

        NSDictionary* attrs = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:text_color.r
                                                                      green:text_color.g
                                                                       blue:text_color.b
                                                                      alpha:text_color.a * m_Alpha]
        };

        NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:nsText
                                                                      attributes:attrs];

        CGFloat ascent = 0, descent = 0, leading = 0;
        CGFloat textWidth = 0, textHeight = 0;

        if (word_wrap) {
            CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)attrStr);

            CGFloat wrapWidth = m_Width - position.x;
            if (wrapWidth < 10.0f) wrapWidth = m_Width;
            CGSize constraints = CGSizeMake(wrapWidth, CGFLOAT_MAX);
            CGSize suggestedSize = CTFramesetterSuggestFrameSizeWithConstraints(
                framesetter, CFRangeMake(0, 0), nullptr, constraints, nullptr);

            textWidth = suggestedSize.width;
            textHeight = suggestedSize.height;
            if (textWidth < 1.0f) textWidth = 1.0f;
            if (textHeight < 1.0f) textHeight = 1.0f;

            CGContextRef bitmapCtx = CGBitmapContextCreate(
                nil, (size_t)textWidth, (size_t)textHeight,
                8, (size_t)textWidth * 4,
                CGColorSpaceCreateDeviceRGB(),
                kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);

            if (bitmapCtx) {
                CGContextSetTextMatrix(bitmapCtx, CGAffineTransformMakeScale(1.0, -1.0));

                CGMutablePathRef path = CGPathCreateMutable();
                CGPathAddRect(path, nullptr, CGRectMake(0, 0, textWidth, textHeight));
                CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, nullptr);
                CTFrameDraw(frame, bitmapCtx);
                CFRelease(path);
                CFRelease(frame);

                CGImageRef cgImage = CGBitmapContextCreateImage(bitmapCtx);
                if (cgImage) {
                    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                                        width:(NSUInteger)textWidth
                                                                                                       height:(NSUInteger)textHeight
                                                                                                    mipmapped:NO];
                    id<MTLTexture> textTexture = [m_Device newTextureWithDescriptor:texDesc];

                    void* pixelData = CGBitmapContextGetData(bitmapCtx);
                    if (pixelData) {
                        MTLRegion region = MTLRegionMake2D(0, 0, (NSUInteger)textWidth, (NSUInteger)textHeight);
                        NSUInteger bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapCtx);
                        [textTexture replaceRegion:region
                                       mipmapLevel:0
                                         withBytes:pixelData
                                       bytesPerRow:bytesPerRow];
                    }

                    [m_CommandEncoder setRenderPipelineState:m_TexturePipeline];
                    [m_CommandEncoder setFragmentTexture:textTexture atIndex:0];
                    [m_CommandEncoder setFragmentSamplerState:m_LinearSampler atIndex:0];

                    matrix_float4x4 model = mat4_identity();
                    model = mat4_translate(model, position.x, position.y);
                    model = mat4_scale(model, textWidth, textHeight);

                    matrix_float4x4 view = {};
                    std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
                    matrix_float4x4 proj = {};
                    std::memcpy(&proj, m_Projection.m, sizeof(proj));
                    matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

                    struct Uniforms {
                        matrix_float4x4 mvp;
                        simd_float4 color;
                        float alpha;
                    };
                    Uniforms uniforms = { mvp, { text_color.r, text_color.g, text_color.b, text_color.a }, m_Alpha };
                    id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                                         length:sizeof(Uniforms)
                                                                        options:MTLResourceStorageModeShared];

                    [m_CommandEncoder setVertexBuffer:m_QuadBuffer offset:0 atIndex:0];
                    [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
                    [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:1];
                    [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

                    CGImageRelease(cgImage);
                }
                CGContextRelease(bitmapCtx);
            }
            CFRelease(framesetter);
        } else {
            CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)attrStr);

            double lineWidth = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
            CGFloat totalHeight = ascent + descent + leading;
            if (totalHeight < 1.0f) totalHeight = 1.0f;

            textWidth = (CGFloat)lineWidth;
            textHeight = totalHeight;

            CGContextRef bitmapCtx = CGBitmapContextCreate(
                nil, (size_t)lineWidth, (size_t)totalHeight,
                8, (size_t)lineWidth * 4,
                CGColorSpaceCreateDeviceRGB(),
                kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);

            if (bitmapCtx) {
                CGContextSetTextPosition(bitmapCtx, 0, descent);
                CTLineDraw(line, bitmapCtx);

                CGImageRef cgImage = CGBitmapContextCreateImage(bitmapCtx);

                if (cgImage) {
                    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                                        width:(NSUInteger)lineWidth
                                                                                                       height:(NSUInteger)totalHeight
                                                                                                    mipmapped:NO];
                    id<MTLTexture> textTexture = [m_Device newTextureWithDescriptor:texDesc];

                    void* pixelData = CGBitmapContextGetData(bitmapCtx);
                    if (pixelData) {
                        MTLRegion region = MTLRegionMake2D(0, 0, (NSUInteger)lineWidth, (NSUInteger)totalHeight);
                        NSUInteger bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapCtx);
                        [textTexture replaceRegion:region
                                       mipmapLevel:0
                                         withBytes:pixelData
                                       bytesPerRow:bytesPerRow];
                    }

                    [m_CommandEncoder setRenderPipelineState:m_TexturePipeline];
                    [m_CommandEncoder setFragmentTexture:textTexture atIndex:0];
                    [m_CommandEncoder setFragmentSamplerState:m_LinearSampler atIndex:0];

                    matrix_float4x4 model = mat4_identity();
                    model = mat4_translate(model, position.x, position.y - descent);
                    model = mat4_scale(model, lineWidth, totalHeight);

                    matrix_float4x4 view = {};
                    std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
                    matrix_float4x4 proj = {};
                    std::memcpy(&proj, m_Projection.m, sizeof(proj));
                    matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

                    struct Uniforms {
                        matrix_float4x4 mvp;
                        simd_float4 color;
                        float alpha;
                    };
                    Uniforms uniforms = { mvp, { text_color.r, text_color.g, text_color.b, text_color.a }, m_Alpha };
                    id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                                         length:sizeof(Uniforms)
                                                                        options:MTLResourceStorageModeShared];

                    [m_CommandEncoder setVertexBuffer:m_QuadBuffer offset:0 atIndex:0];
                    [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
                    [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:1];
                    [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

                    CGImageRelease(cgImage);
                }

                CGContextRelease(bitmapCtx);
            }

            CFRelease(line);
        }
    }
}

void metal_renderer::draw_text_aligned(const std::string& text, const rectangle& bounds, const color& text_color,
                                       text_alignment h_align, vertical_alignment v_align,
                                       float font_size, const std::string& font_family) {
    if (text.empty()) return;

    float text_width = measure_text_width(text, font_size, font_family);
    float text_height = font_size;

    float x = bounds.x;
    float y = bounds.y;

    switch (h_align) {
        case text_alignment::left:   x = bounds.x; break;
        case text_alignment::center: x = bounds.x + (bounds.width - text_width) * 0.5f; break;
        case text_alignment::right:  x = bounds.x + bounds.width - text_width; break;
    }
    switch (v_align) {
        case vertical_alignment::top:    y = bounds.y; break;
        case vertical_alignment::center: y = bounds.y + (bounds.height - text_height) * 0.5f; break;
        case vertical_alignment::bottom: y = bounds.y + bounds.height - text_height; break;
    }

    draw_text(text, { x, y }, text_color, font_size, font_family);
}

void metal_renderer::draw_image(const std::string& image_path, const rectangle& destination) {
    @autoreleasepool {
        NSString* nsPath = [NSString stringWithUTF8String:image_path.c_str()];
        NSURL* url = [NSURL fileURLWithPath:nsPath];
        if (!url) return;

        CIImage* ciImage = [CIImage imageWithContentsOfURL:url];
        if (!ciImage) return;

        CIContext* ciContext = [CIContext contextWithOptions:nil];
        CGImageRef cgImage = [ciContext createCGImage:ciImage fromRect:ciImage.extent];
        if (!cgImage) return;

        size_t imgW = CGImageGetWidth(cgImage);
        size_t imgH = CGImageGetHeight(cgImage);

        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                            width:imgW
                                                                                           height:imgH
                                                                                        mipmapped:NO];
        id<MTLTexture> texture = [m_Device newTextureWithDescriptor:texDesc];

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef bitmapCtx = CGBitmapContextCreate(
            nil, imgW, imgH, 8, imgW * 4, colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);

        if (bitmapCtx) {
            CGContextDrawImage(bitmapCtx, CGRectMake(0, 0, imgW, imgH), cgImage);
            void* pixelData = CGBitmapContextGetData(bitmapCtx);
            if (pixelData) {
                MTLRegion region = MTLRegionMake2D(0, 0, imgW, imgH);
                [texture replaceRegion:region mipmapLevel:0
                            withBytes:pixelData bytesPerRow:imgW * 4];
            }
            CGContextRelease(bitmapCtx);
        }
        CGColorSpaceRelease(colorSpace);
        CGImageRelease(cgImage);

        [m_CommandEncoder setRenderPipelineState:m_TexturePipeline];
        [m_CommandEncoder setFragmentTexture:texture atIndex:0];
        [m_CommandEncoder setFragmentSamplerState:m_LinearSampler atIndex:0];

        matrix_float4x4 model = mat4_identity();
        model = mat4_translate(model, destination.x, destination.y);
        model = mat4_scale(model, destination.width, destination.height);

        matrix_float4x4 view = {};
        std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
        matrix_float4x4 proj = {};
        std::memcpy(&proj, m_Projection.m, sizeof(proj));
        matrix_float4x4 mvp = matrix_multiply(matrix_multiply(proj, view), model);

        struct Uniforms {
            matrix_float4x4 mvp;
            simd_float4 color;
            float alpha;
        };
        Uniforms uniforms = { mvp, { 1.0f, 1.0f, 1.0f, 1.0f }, m_Alpha };
        id<MTLBuffer> uniformBuffer = [m_Device newBufferWithBytes:&uniforms
                                                             length:sizeof(Uniforms)
                                                            options:MTLResourceStorageModeShared];

        [m_CommandEncoder setVertexBuffer:m_QuadBuffer offset:0 atIndex:0];
        [m_CommandEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
        [m_CommandEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:1];
        [m_CommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    }
}

void metal_renderer::draw_image_subregion(const std::string& image_path, const rectangle& source, const rectangle& destination) {
    draw_image(image_path, destination);
}

void metal_renderer::push_transform(float x, float y, float rotation, float scale_x, float scale_y) {
    m_TransformStack.push_back(m_CurrentTransform);

    matrix_float4x4 view = {};
    std::memcpy(&view, m_CurrentTransform.m, sizeof(view));
    view = mat4_translate(view, x, y);

    if (rotation != 0.0f) {
        float rad = rotation * M_PI / 180.0f;
        matrix_float4x4 rot = mat4_identity();
        rot.columns[0] = (simd_float4){ cosf(rad), sinf(rad), 0, 0 };
        rot.columns[1] = (simd_float4){ -sinf(rad), cosf(rad), 0, 0 };
        view = matrix_multiply(view, rot);
    }
    view = mat4_scale(view, scale_x, scale_y);
    std::memcpy(m_CurrentTransform.m, &view, sizeof(m_CurrentTransform));
}

void metal_renderer::pop_transform() {
    if (m_TransformStack.empty()) return;
    m_CurrentTransform = m_TransformStack.back();
    m_TransformStack.pop_back();
}

void metal_renderer::set_blend_mode(bool enabled) {
    m_BlendEnabled = enabled;
}

void metal_renderer::set_alpha(float alpha) {
    m_Alpha = std::clamp(alpha, 0.0f, 1.0f);
}

void metal_renderer::get_viewport_size(uint32_t& width, uint32_t& height) const {
    width = m_Width;
    height = m_Height;
}

float metal_renderer::measure_text_width(const std::string& text, float font_size,
                                          const std::string& font_family) {
    if (text.empty()) return 0.0f;

    @autoreleasepool {
        NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
        NSString* nsFontFamily = font_family.empty()
            ? @"Arial" : [NSString stringWithUTF8String:font_family.c_str()];

        NSFont* font = [NSFont fontWithName:nsFontFamily size:font_size];
        if (!font) {
            font = [NSFont systemFontOfSize:font_size];
        }

        NSDictionary* attrs = @{ NSFontAttributeName: font };
        NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:nsText
                                                                      attributes:attrs];
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)attrStr);
        CGFloat width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
        CFRelease(line);
        return static_cast<float>(width);
    }
}

bool metal_renderer::init_metal() {
    @autoreleasepool {
        m_Device = MTLCreateSystemDefaultDevice();
        if (!m_Device) return false;

        m_CommandQueue = [m_Device newCommandQueue];
        if (!m_CommandQueue) return false;

        m_MetalLayer.device = m_Device;
        m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_MetalLayer.framebufferOnly = YES;

        CGSize drawableSize = m_MetalLayer.drawableSize;
        m_Width = static_cast<uint32_t>(drawableSize.width);
        m_Height = static_cast<uint32_t>(drawableSize.height);

        return true;
    }
}

bool metal_renderer::init_buffers() {
    @autoreleasepool {
        const float quadVerts[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f
        };
        m_QuadBuffer = [m_Device newBufferWithBytes:quadVerts
                                              length:sizeof(quadVerts)
                                             options:MTLResourceStorageModeShared];
        if (!m_QuadBuffer) return false;

        const int segments = 64;
        struct Vec2 { float x, y; };
        std::vector<Vec2> circleVerts;
        circleVerts.push_back({0, 0});
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * M_PI * i / segments;
            circleVerts.push_back({cosf(angle), sinf(angle)});
        }
        m_CircleBuffer = [m_Device newBufferWithBytes:circleVerts.data()
                                                length:circleVerts.size() * sizeof(Vec2)
                                               options:MTLResourceStorageModeShared];
        if (!m_CircleBuffer) return false;

        return true;
    }
}

bool metal_renderer::init_pipeline_states() {
    @autoreleasepool {
        NSError* error = nil;
        NSString* libPath = [[NSBundle mainBundle] pathForResource:@"default" ofType:@"metallib"];
        if (!libPath) {
            libPath = @"./default.metallib";
        }

        id<MTLLibrary> library = [m_Device newLibraryWithFile:libPath error:&error];
        if (!library) {
            library = [m_Device newDefaultLibrary];
        }
        if (!library) return false;

        {
            id<MTLFunction> vertFn = [library newFunctionWithName:@"basic_vertex"];
            id<MTLFunction> fragFn = [library newFunctionWithName:@"basic_fragment"];
            if (!vertFn || !fragFn) return false;

            MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
            desc.vertexFunction = vertFn;
            desc.fragmentFunction = fragFn;
            desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            desc.colorAttachments[0].blendingEnabled = YES;
            desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

            m_BasicPipeline = [m_Device newRenderPipelineStateWithDescriptor:desc error:&error];
            if (!m_BasicPipeline) {
                NSLog(@"Failed to create basic pipeline: %@", error);
                return false;
            }
        }

        {
            id<MTLFunction> vertFn = [library newFunctionWithName:@"texture_vertex"];
            id<MTLFunction> fragFn = [library newFunctionWithName:@"texture_fragment"];
            if (!vertFn || !fragFn) return false;

            MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
            desc.vertexFunction = vertFn;
            desc.fragmentFunction = fragFn;
            desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            desc.colorAttachments[0].blendingEnabled = YES;
            desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

            m_TexturePipeline = [m_Device newRenderPipelineStateWithDescriptor:desc error:&error];
            if (!m_TexturePipeline) {
                NSLog(@"Failed to create texture pipeline: %@", error);
                return false;
            }
        }

        return true;
    }
}

bool metal_renderer::init_sampler_states() {
    @autoreleasepool {
        MTLSamplerDescriptor* desc = [MTLSamplerDescriptor new];
        desc.minFilter = MTLSamplerMinMagFilterLinear;
        desc.magFilter = MTLSamplerMinMagFilterLinear;
        desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
        desc.tAddressMode = MTLSamplerAddressModeClampToEdge;

        m_LinearSampler = [m_Device newSamplerStateWithDescriptor:desc];
        if (!m_LinearSampler) return false;

        desc.minFilter = MTLSamplerMinMagFilterNearest;
        desc.magFilter = MTLSamplerMinMagFilterNearest;
        m_NearestSampler = [m_Device newSamplerStateWithDescriptor:desc];

        return true;
    }
}

void metal_renderer::release_metal_resources() {
    @autoreleasepool {
        m_BasicPipeline = nil;
        m_TextPipeline = nil;
        m_TexturePipeline = nil;
        m_LinearSampler = nil;
        m_NearestSampler = nil;
        m_QuadBuffer = nil;
        m_CircleBuffer = nil;
        m_DepthState = nil;
        m_CurrentDrawableTexture = nil;
        m_Drawable = nil;
        m_CommandBuffer = nil;
        m_CommandEncoder = nil;
        m_CommandQueue = nil;
    }
}

std::shared_ptr<renderer> renderer::create_metal_renderer() {
    return std::make_shared<metal_renderer>();
}

} // namespace spiration
