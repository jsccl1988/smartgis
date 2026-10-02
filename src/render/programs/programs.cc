// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/programs/programs.h"

namespace render {
namespace programs {
namespace {

// Copied verbatim from GraphicsCache shader literals (kVsTextured / kPsTextured
// / kVsSolid / kPsSolid / kVsLitSolid / kPsLitSolid).

const char* kVsTextured = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 world = mul(view, float4(input.pos, 1.0));
    output.pos = mul(proj, world);
    output.uv = input.uv;
    return output;
}
)";

const char* kPsTextured = R"(
Texture2D base_color_texture : register(t0);
SamplerState linear_sampler : register(s0);

cbuffer ColorCB : register(b1)
{
    float4 tint;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSIn input) : SV_TARGET
{
    float4 tex = base_color_texture.Sample(linear_sampler, input.uv);
    return float4(tex.rgb * tint.rgb, tex.a * tint.a);
}
)";

const char* kVsSolid = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

float4 main(float3 pos : POSITION) : SV_POSITION
{
    float4 world = mul(view, float4(pos, 1.0));
    return mul(proj, world);
}
)";

const char* kPsSolid = R"(
cbuffer ColorCB : register(b1)
{
    float4 color;
};

float4 main() : SV_TARGET
{
    return color;
}
)";

const char* kVsLitSolid = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 nrm : TEXCOORD0;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 eye = mul(view, float4(input.pos, 1.0));
    output.pos = mul(proj, eye);
    // Mesh positions are world-space; pass world normals for Lambert.
    output.nrm = input.nrm;
    return output;
}
)";

const char* kPsLitSolid = R"(
cbuffer ColorCB : register(b1)
{
    float4 color;
};
cbuffer LightCB : register(b2)
{
    float dir_x;
    float dir_y;
    float dir_z;
    float ambient;
    float color_r;
    float color_g;
    float color_b;
    float intensity;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 nrm : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET
{
    float3 N = normalize(input.nrm);
    float3 L = normalize(-float3(dir_x, dir_y, dir_z));
    float ndotl = saturate(dot(N, L));
    float3 light_rgb = float3(color_r, color_g, color_b) * intensity;
    float3 lit = color.rgb * (ambient + light_rgb * ndotl);
    return float4(lit, color.a);
}
)";

// Lit DEM albedo: PositionUv layout, screen-space normals, GGX specular,
// sun self-shadow, and a two-tap derivative AA. Stride stays 5 floats.
const char* kVsLitTextured = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1;
    float3 eye : TEXCOORD2;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 clip = mul(view, float4(input.pos, 1.0));
    output.pos = mul(proj, clip);
    output.uv = input.uv;
    output.world = input.pos;
    // Column-major view: translation is -R * eye.
    float3 t = float3(view._14, view._24, view._34);
    float3x3 R = (float3x3)view;
    output.eye = -mul(transpose(R), t);
    return output;
}
)";

const char* kPsLitTextured = R"(
Texture2D base_color_texture : register(t0);
SamplerState linear_sampler : register(s0);

cbuffer ColorCB : register(b1)
{
    float4 tint;
};
cbuffer LightCB : register(b2)
{
    float dir_x;
    float dir_y;
    float dir_z;
    float ambient;
    float color_r;
    float color_g;
    float color_b;
    float intensity;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1;
    float3 eye : TEXCOORD2;
};

float4 main(PSIn input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 tex = base_color_texture.Sample(linear_sampler, uv);
    // Two-tap derivative AA. The RHI swapchain is 1x; this softens the
    // draped albedo without a second color target.
    float2 uv_px = float2(abs(ddx(uv).x) + abs(ddy(uv).x),
                          abs(ddx(uv).y) + abs(ddy(uv).y));
    float4 tex_b = base_color_texture.Sample(linear_sampler, uv + 0.5 * uv_px);
    float3 albedo = tint.rgb * 0.5 * (tex.rgb + tex_b.rgb);

    float3 dpdx = ddx(input.world);
    float3 dpdy = ddy(input.world);
    float3 N = normalize(cross(dpdx, dpdy));
    if (N.y < 0.0)
        N = -N;
    float3 L = normalize(-float3(dir_x, dir_y, dir_z));
    float ndotl_raw = dot(N, L);
    // Soft self-shadow: wide smoothstep kills DEM facet sparkle.
    float sun_shadow = smoothstep(-0.20, 0.55, ndotl_raw);
    float ao = saturate(0.58 + 0.42 * N.y);

    float3 V = normalize(input.eye - input.world);
    float3 H = normalize(L + V);
    float ndotv = saturate(dot(N, V));
    float ndoth = saturate(dot(N, H));
    float ndotl = saturate(ndotl_raw);
    // Flats (vegetation) stay rough; steeper rock tightens the lobe.
    float rough = saturate(0.78 - 0.28 * (1.0 - N.y));
    float a2 = rough * rough;
    a2 = a2 * a2;
    float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
    float D = a2 / max(3.14159 * d * d, 1e-4);
    float fres = pow(1.0 - ndotv, 5.0);
    float spec = D * (0.03 + 0.70 * fres) * sun_shadow;

    float3 light_rgb = float3(color_r, color_g, color_b) * intensity;
    float3 diffuse = albedo * (ambient * ao + light_rgb * ndotl * sun_shadow);
    float3 lit = diffuse + light_rgb * spec * 0.045;
    return float4(saturate(lit), tex.a * tint.a);
}
)";

constexpr uint32_t kCameraBytes = 128;

const rhi::BindingSlot kSolidBindings[] = {
    {.slot = kCameraSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kVertex,
     .size_bytes = kCameraBytes,
     .hlsl_name = "CameraCB"},
    {.slot = kColorSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = sizeof(Color),
     .hlsl_name = "ColorCB"},
};

const rhi::BindingSlot kTexturedBindings[] = {
    {.slot = kCameraSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kVertex,
     .size_bytes = kCameraBytes,
     .hlsl_name = "CameraCB"},
    {.slot = kColorSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = sizeof(Color),
     .hlsl_name = "ColorCB"},
    {.slot = kTextureSlot,
     .kind = rhi::BindingKind::kSrv,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = 0,
     .hlsl_name = "base_color_texture"},
    {.slot = kTextureSlot,
     .kind = rhi::BindingKind::kSampler,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = 0,
     .hlsl_name = "linear_sampler"},
};

const rhi::BindingSlot kLitBindings[] = {
    {.slot = kCameraSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kVertex,
     .size_bytes = kCameraBytes,
     .hlsl_name = "CameraCB"},
    {.slot = kColorSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = sizeof(Color),
     .hlsl_name = "ColorCB"},
    {.slot = kLightSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = sizeof(Light),
     .hlsl_name = "LightCB"},
};

const rhi::BindingSlot kLitTexturedBindings[] = {
    {.slot = kCameraSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kVertex,
     .size_bytes = kCameraBytes,
     .hlsl_name = "CameraCB"},
    {.slot = kColorSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = sizeof(Color),
     .hlsl_name = "ColorCB"},
    {.slot = kLightSlot,
     .kind = rhi::BindingKind::kConstantBuffer,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = sizeof(Light),
     .hlsl_name = "LightCB"},
    {.slot = kTextureSlot,
     .kind = rhi::BindingKind::kSrv,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = 0,
     .hlsl_name = "base_color_texture"},
    {.slot = kTextureSlot,
     .kind = rhi::BindingKind::kSampler,
     .stage = rhi::ShaderStage::kPixel,
     .size_bytes = 0,
     .hlsl_name = "linear_sampler"},
};

rhi::GraphicsPipelineDesc make_desc(const char* vs, const char* ps,
                                    rhi::VertexLayout layout,
                                    const rhi::BindingSlot* bindings,
                                    uint32_t binding_count) {
  rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = vs;
  desc.pixel.hlsl = ps;
  desc.vertex_layout = layout;
  desc.bindings = bindings;
  desc.binding_count = binding_count;
  desc.blend = rhi::BlendMode::kOpaque;
  desc.compile_depth_off = true;
  desc.compile_depth_write = true;
  desc.compile_depth_test = false;
  desc.camera_slot = static_cast<int32_t>(kCameraSlot);
  return desc;
}

}  // namespace

rhi::GraphicsPipelineDesc solid_pipeline_desc() {
  rhi::GraphicsPipelineDesc desc =
      make_desc(kVsSolid, kPsSolid, rhi::VertexLayout::kPosition, kSolidBindings,
                2);
  // Fog records DepthMode::kTestOnly. Without this variant FlyCube skips the draw.
  desc.compile_depth_test = true;
  return desc;
}

rhi::GraphicsPipelineDesc textured_pipeline_desc() {
  return make_desc(kVsTextured, kPsTextured, rhi::VertexLayout::kPositionUv,
                   kTexturedBindings, 4);
}

rhi::GraphicsPipelineDesc lit_pipeline_desc() {
  return make_desc(kVsLitSolid, kPsLitSolid, rhi::VertexLayout::kPositionNormal,
                   kLitBindings, 3);
}

rhi::GraphicsPipelineDesc lit_textured_pipeline_desc() {
  return make_desc(kVsLitTextured, kPsLitTextured,
                   rhi::VertexLayout::kPositionUv, kLitTexturedBindings, 5);
}

}  // namespace programs
}  // namespace render
