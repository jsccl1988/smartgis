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

}  // namespace programs
}  // namespace render
