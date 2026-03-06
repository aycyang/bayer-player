#include <metal_stdlib>
using namespace metal;

struct VertexOutput {
  float4 position [[position]];
  float2 uv;
};

struct Uniforms {
  int frame_index;
  int show_mask;
  int show_orig;
};

fragment float4 fs_main(VertexOutput in [[stage_in]],
                        constant Uniforms& uniforms [[buffer(0)]],
                        texture2d_array<float> tex [[texture(0)]],
                        sampler texSampler [[sampler(0)]],
                        texture2d<float> imgTex [[texture(1)]],
                        sampler imgTexSampler [[sampler(1)]]) {
  uint2 imgTexCoord = uint2(imgTex.get_width() * in.uv.x, imgTex.get_height() * in.uv.y);
  uint2 maskTexCoord = uint2(imgTexCoord.x % tex.get_width(), imgTexCoord.y % tex.get_height());

  float4 imgTexel = imgTex.read(imgTexCoord);
  float4 maskTexel = tex.read(maskTexCoord, uniforms.frame_index);

  float value = max(max(imgTexel.r, imgTexel.g), imgTexel.b);
  float on = value > maskTexel.r;
  float4 final = float4(on, on, on, 1);

  if (uniforms.show_mask) {
    return float4(float3(maskTexel.r), 1);
  } else if (uniforms.show_orig) {
    return imgTexel;
  } else {
    return final;
  }
}
