#include <metal_stdlib>
using namespace metal;

struct VertexOutput
{
    float4 position [[position]];
    float2 uv;
};

struct Uniforms
{
    float4 color;
};

fragment float4 fs_main(
  VertexOutput in [[stage_in]],
  constant Uniforms& uniforms [[buffer(0)]],
  texture3d<float> tex [[texture(0)]],
  sampler texSampler [[sampler(0)]],
  texture2d<float> imgTex [[texture(1)]],
  sampler imgTexSampler [[sampler(1)]]
)
{
    float4 sample = imgTex.sample(imgTexSampler, in.uv);
    return sample * uniforms.color;
}
