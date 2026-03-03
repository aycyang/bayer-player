#include <metal_stdlib>
using namespace metal;

struct VertexOutput
{
    float4 position [[position]];
    float2 uv;
};

fragment float4 fs_main(
  VertexOutput in [[stage_in]],
  texture2d<float> tex [[texture(0)]],
  sampler texSampler [[sampler(0)]]
)
{
    return tex.sample(texSampler, in.uv);
}
