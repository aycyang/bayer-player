#include <metal_stdlib>
using namespace metal;

struct VertexOutput
{
    float4 position [[position]];
    float2 uv;
};

struct Uniforms
{
    int frame_index;
};

fragment float4 fs_main(
  VertexOutput in [[stage_in]],
  constant Uniforms& uniforms [[buffer(0)]],
  texture2d_array<float> tex [[texture(0)]],
  sampler texSampler [[sampler(0)]],
  texture2d<float> imgTex [[texture(1)]],
  sampler imgTexSampler [[sampler(1)]]
)
{
    float4 sample = imgTex.sample(imgTexSampler, in.uv);
    return tex.sample(texSampler, in.uv, uniforms.frame_index);
    return sample * uniforms.frame_index;
}
