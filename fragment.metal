#include <metal_stdlib>
using namespace metal;

struct VertexOutput
{
    float4 position [[position]];
    float4 color;
};

fragment float4 fs_main(VertexOutput in [[stage_in]])
{
    return in.color;
}
