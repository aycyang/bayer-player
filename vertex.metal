#include <metal_stdlib>
using namespace metal;

struct VertexInput
{
    float2 position [[attribute(0)]];
    float4 color    [[attribute(1)]];
};

struct VertexOutput
{
    float4 position [[position]];
    float4 color;
};

vertex VertexOutput vs_main(VertexInput in [[stage_in]])
{
    VertexOutput out;
    out.position = float4(in.position, 0.0, 1.0);
    out.color = in.color;
    return out;
}
