#include <metal_stdlib>
using namespace metal;

struct VertexOutput
{
    float4 position [[position]];
    float2 uv;
};

fragment float4 fs_main(
  VertexOutput in [[stage_in]],
  texture3d<float> tex [[texture(0)]],
  sampler texSampler [[sampler(0)]],
  texture2d<float> imgTex [[texture(1)]],
  sampler imgTexSampler [[sampler(1)]]
)
{
    return imgTex.sample(imgTexSampler, in.uv);
}
