// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Shared solid / textured / lit graphics programs. HLSL matches the former
// FlyCube GraphicsCache shaders. Callers create a Pipeline from the desc and
// upload Color / Light with set_constants.

#ifndef RENDER_PROGRAMS_PROGRAMS_H_
#define RENDER_PROGRAMS_PROGRAMS_H_

#include <cstddef>
#include <cstdint>

#include "render/render_export.h"
#include "render/rhi/rhi.h"

namespace render {
namespace programs {

// Slot 0 is the camera (bind_camera writes 128 bytes when camera_slot is 0).
// Slot 1 is the unlit or albedo color. Slot 2 is the lit pixel LightCB.
// Textured sampling uses SRV slot 0 (base_color_texture) and one sampler
// (linear_sampler, register s0).
inline constexpr uint32_t kCameraSlot = 0;
inline constexpr uint32_t kColorSlot = 1;
inline constexpr uint32_t kLightSlot = 2;
inline constexpr uint32_t kTextureSlot = 0;

// 16-byte color constant for slot 1. Layout is one float4 (r, g, b, a).
struct Color {
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 0.f;
};

// 32-byte light constant for slot 2. Field order matches the lit pixel
// LightCB (dir xyz, ambient, color rgb, intensity) and the former
// LightParams{} defaults.
struct Light {
  float dir_x = -0.4f;
  float dir_y = -0.8f;
  float dir_z = -0.35f;
  float ambient = 0.25f;
  float color_r = 1.f;
  float color_g = 1.f;
  float color_b = 1.f;
  float intensity = 1.f;
};

static_assert(sizeof(Color) == 16, "color constant is one float4");
static_assert(offsetof(Color, a) == 12, "color alpha follows rgb");
static_assert(sizeof(Light) == 32, "light constant matches LightCB");
static_assert(offsetof(Light, dir_x) == 0, "light dir starts the cbuffer");
static_assert(offsetof(Light, ambient) == 12, "ambient follows dir xyz");
static_assert(offsetof(Light, color_r) == 16, "light color follows ambient");
static_assert(offsetof(Light, intensity) == 28, "intensity is the last float");

RENDER_EXPORT rhi::GraphicsPipelineDesc solid_pipeline_desc();
RENDER_EXPORT rhi::GraphicsPipelineDesc textured_pipeline_desc();
RENDER_EXPORT rhi::GraphicsPipelineDesc lit_pipeline_desc();

}  // namespace programs
}  // namespace render

#endif  // RENDER_PROGRAMS_PROGRAMS_H_
