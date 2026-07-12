#include <metal_stdlib>
using namespace metal;

struct BasicVertexIn {
    float2 position [[attribute(0)]];
};

struct BasicVertexOut {
    float4 position [[position]];
};

vertex BasicVertexOut basic_vertex(
    BasicVertexIn in [[stage_in]],
    constant float4x4& uMVP [[buffer(1)]]
) {
    BasicVertexOut out;
    out.position = uMVP * float4(in.position, 0.0, 1.0);
    return out;
}

fragment float4 basic_fragment(
    BasicVertexOut in [[stage_in]],
    constant float4& uColor [[buffer(0)]]
) {
    return uColor;
}

struct TextureVertexIn {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct TextureVertexOut {
    float4 position [[position]];
    float2 texCoord [[user(texturecoord)]];
};

vertex TextureVertexOut texture_vertex(
    TextureVertexIn in [[stage_in]],
    constant float4x4& uMVP [[buffer(1)]]
) {
    TextureVertexOut out;
    out.position = uMVP * float4(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 texture_fragment(
    TextureVertexOut in [[stage_in]],
    texture2d<float> uTexture [[texture(0)]],
    sampler uSampler [[sampler(0)]],
    constant float4& uColor [[buffer(0)]],
    constant float& uAlpha [[buffer(1)]]
) {
    float4 texColor = uTexture.sample(uSampler, in.texCoord);
    return float4(texColor.rgb * uColor.rgb, texColor.a * uColor.a * uAlpha);
}
