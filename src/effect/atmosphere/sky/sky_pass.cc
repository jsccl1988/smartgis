// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/sky/sky_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "effect/atmosphere/detail/math.h"
#include "effect/atmosphere/detail/mesh.h"
#include "effect/atmosphere/detail/raster.h"
#include "effect/atmosphere/sky/constants.h"
#include "effect/atmosphere/sky/hlsl.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {
namespace {

constexpr uint32_t kSkyConstantSlot = 1;

render::rhi::GraphicsPipelineDesc sky_graphics_desc() {
  // Fullscreen NDC sky: CameraCB on PS for view-ray unproject, SkyCB for tint.
  static constexpr render::rhi::BindingSlot kBindings[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 128,
       .hlsl_name = "CameraCB"},
      {.slot = kSkyConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = sizeof(SkyConstants),
       .hlsl_name = "SkyCB"},
  };
  render::rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = kVsSky;
  desc.pixel.hlsl = kPsSky;
  desc.vertex_layout = render::rhi::VertexLayout::kPosition;
  desc.bindings = kBindings;
  desc.binding_count = sizeof(kBindings) / sizeof(kBindings[0]);
  desc.blend = render::rhi::BlendMode::kOpaque;
  // Backdrop only: never write depth so ocean/terrain always composite on top.
  desc.compile_depth_off = true;
  desc.compile_depth_write = false;
  desc.compile_depth_test = false;
  desc.camera_slot = 0;
  return desc;
}

}  // namespace

SkyPass::SkyPass() = default;

SkyPass::~SkyPass() {
  // FlyCube Device may already be shut down; abandon handles only.
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  release();
}

void SkyPass::set_params(const SkyDrawParams& params) {
  params_ = params;
  detail::normalize3(&params_.sun_x, &params_.sun_y, &params_.sun_z);
}

void SkyPass::set_sun_direction(float x, float y, float z) {
  params_.sun_x = x;
  params_.sun_y = y;
  params_.sun_z = z;
  detail::normalize3(&params_.sun_x, &params_.sun_y, &params_.sun_z);
}

void SkyPass::set_sun_from_azimuth_elevation(float azimuth_rad,
                                             float elevation_rad) {
  detail::sun_from_azimuth_elevation(azimuth_rad, elevation_rad, &params_.sun_x,
                                     &params_.sun_y, &params_.sun_z);
}

void SkyPass::sample_sky_rgb(const SkyDrawParams& p, float dir_x, float dir_y,
                             float dir_z, float* out_r, float* out_g,
                             float* out_b) {
  float dx = dir_x;
  float dy = dir_y;
  float dz = dir_z;
  detail::normalize3(&dx, &dy, &dz);

  float sun_x = p.sun_x;
  float sun_y = p.sun_y;
  float sun_z = p.sun_z;
  detail::normalize3(&sun_x, &sun_y, &sun_z);

  const float elev = detail::clampf(sun_y, -1.0f, 1.0f);
  const float day = detail::clampf(elev * 1.35f + 0.55f, 0.0f, 1.0f);
  float sunset_r = detail::lerp(p.sunset_r, p.horizon_r, 0.55f);
  float sunset_g = detail::lerp(p.sunset_g, p.horizon_g, 0.55f);
  float sunset_b = detail::lerp(p.sunset_b, p.horizon_b, 0.55f);
  sunset_r = (std::min)(sunset_r, sunset_b * 0.85f);
  const float hr = detail::lerp(sunset_r, p.horizon_r, day);
  const float hg = detail::lerp(sunset_g, p.horizon_g, day);
  const float hb = detail::lerp(sunset_b, p.horizon_b, day);

  // Matches kPsSky: elevation blend + haze + Bruneton-lite + sun disk/corona.
  const float elev_v = detail::clampf(dy, 0.0f, 1.0f);
  const float blend = std::pow(detail::clampf(elev_v * 1.45f, 0.0f, 1.0f), 0.38f);
  float r = detail::lerp(hr, p.zenith_r, blend);
  float g = detail::lerp(hg, p.zenith_g, blend);
  float b = detail::lerp(hb, p.zenith_b, blend);
  const float haze = 1.0f - elev_v;
  const float haze2 = haze * haze * 0.02f;
  r = detail::clampf(r + hr * haze2, 0.0f, 1.0f);
  g = detail::clampf(g + hg * haze2, 0.0f, 1.0f);
  b = detail::clampf(b + hb * haze2, 0.0f, 1.0f);

  // Bruneton-lite analytical multi-scatter tint (no LUT tables).
  const float rayleigh = std::pow(elev_v, 0.55f);
  r *= detail::lerp(1.0f, 0.72f, rayleigh);
  g *= detail::lerp(1.0f, 0.88f, rayleigh);
  b *= detail::lerp(1.0f, 1.22f, rayleigh);
  float dir_hx = dx;
  float dir_hz = dz;
  float sun_hx = sun_x;
  float sun_hz = sun_z;
  float dir_hy = 1.0e-3f;
  float sun_hy = 1.0e-3f;
  detail::normalize3(&dir_hx, &dir_hy, &dir_hz);
  detail::normalize3(&sun_hx, &sun_hy, &sun_hz);
  const float azi =
      detail::clampf(dir_hx * sun_hx + dir_hy * sun_hy + dir_hz * sun_hz, 0.0f,
                     1.0f);
  const float mie_warm =
      std::pow(azi, 2.0f) * haze *
      detail::clampf(1.0f - std::fabs(elev) * 0.55f, 0.0f, 1.0f) * 0.35f;
  r = detail::clampf(r + 0.03f * mie_warm, 0.0f, 1.0f);
  g = detail::clampf(g + 0.02f * mie_warm, 0.0f, 1.0f);
  b = detail::clampf(b + 0.015f * mie_warm, 0.0f, 1.0f);
  const float twilight = detail::clampf(1.0f - std::fabs(elev) * 3.5f, 0.0f, 1.0f);
  const float ozone =
      twilight * detail::clampf(1.0f - elev_v * 1.35f, 0.0f, 1.0f) *
      (0.15f + 0.35f * azi) * 0.25f;
  r = detail::clampf(r + 0.015f * ozone, 0.0f, 1.0f);
  g = detail::clampf(g + 0.01f * ozone, 0.0f, 1.0f);
  b = detail::clampf(b + 0.04f * ozone, 0.0f, 1.0f);

  const float sun_dot =
      detail::clampf(dx * sun_x + dy * sun_y + dz * sun_z, 0.0f, 1.0f);
  const float disk =
      std::pow(sun_dot, 256.0f) * p.sun_glow_strength * 1.55f;
  const float corona =
      std::pow(sun_dot, 12.0f) * p.sun_glow_strength * 0.42f;
  r = detail::clampf(r + disk * 1.0f + corona * 1.0f, 0.0f, 1.0f);
  g = detail::clampf(g + disk * 0.96f + corona * 0.78f, 0.0f, 1.0f);
  b = detail::clampf(b + disk * 0.88f + corona * 0.48f, 0.0f, 1.0f);

  if (out_r) {
    *out_r = r;
  }
  if (out_g) {
    *out_g = g;
  }
  if (out_b) {
    *out_b = b;
  }
}

void SkyPass::average_sky_rgb(const SkyDrawParams& p, float* out_r,
                              float* out_g, float* out_b) {
  float zr = 0.f;
  float zg = 0.f;
  float zb = 0.f;
  float hr = 0.f;
  float hg = 0.f;
  float hb = 0.f;
  float sr = 0.f;
  float sg = 0.f;
  float sb = 0.f;
  sample_sky_rgb(p, 0.f, 1.f, 0.f, &zr, &zg, &zb);
  sample_sky_rgb(p, 0.f, 0.05f, 1.f, &hr, &hg, &hb);
  sample_sky_rgb(p, p.sun_x, p.sun_y, p.sun_z, &sr, &sg, &sb);
  if (out_r) {
    *out_r = (zr + hr + sr) / 3.0f;
  }
  if (out_g) {
    *out_g = (zg + hg + sg) / 3.0f;
  }
  if (out_b) {
    *out_b = (zb + hb + sb) / 3.0f;
  }
}

void SkyPass::destroy_pipeline() {
  if (pipeline_ && pipeline_device_) {
    pipeline_device_->destroy_pipeline(pipeline_);
  }
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool SkyPass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_ && pipeline_device_ == device) {
    return true;
  }
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(sky_graphics_desc());
  return pipeline_ != nullptr;
}

bool SkyPass::ensure_dome_mesh(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (detail::static_mesh_ready(device_, vertex_, index_, device) &&
      index_count_ >= 3) {
    return true;
  }
  // Oversized NDC triangle covering the clip volume (same as FogPass).
  const float ndc[9] = {
      -1.f, -1.f, 0.f, 3.f, -1.f, 0.f, -1.f, 3.f, 0.f,
  };
  const uint32_t indices[3] = {0, 1, 2};
  const bool ok = detail::upload_static_mesh(
      &device_, &vertex_, &index_, &index_count_, device, ndc,
      static_cast<uint32_t>(sizeof(ndc)), indices,
      static_cast<uint32_t>(sizeof(indices)));
  if (ok) {
    built_radius_ = 0.f;
  }
  return ok;
}

bool SkyPass::record(render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
                     uint32_t height, const render::rhi::CameraMatrices* camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!ensure_dome_mesh(device)) {
    return false;
  }
  if (!ensure_pipeline(device)) {
    return false;
  }

  SkyConstants sky{};
  sky.sun_x = params_.sun_x;
  sky.sun_y = params_.sun_y;
  sky.sun_z = params_.sun_z;
  sky.zenith_r = params_.zenith_r;
  sky.zenith_g = params_.zenith_g;
  sky.zenith_b = params_.zenith_b;
  sky.horizon_r = params_.horizon_r;
  sky.horizon_g = params_.horizon_g;
  sky.horizon_b = params_.horizon_b;
  sky.sunset_r = params_.sunset_r;
  sky.sunset_g = params_.sunset_g;
  sky.sunset_b = params_.sunset_b;
  sky.sun_glow = params_.sun_glow_strength;
  if (camera) {
    detail::eye_from_view(camera->view, &sky.cam_x, &sky.cam_y, &sky.cam_z);
  }

  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  detail::apply_raster(
      list, {pipeline_, render::rhi::BlendMode::kOpaque,
             render::rhi::DepthMode::kDisabled});
  list->set_constants(kSkyConstantSlot, &sky,
                      static_cast<uint32_t>(sizeof(sky)));
  detail::draw_indexed_mesh(list, vertex_, index_, 3 * sizeof(float),
                            index_count_);
  return true;
}

void SkyPass::release() {
  detail::release_static_mesh(&device_, &vertex_, &index_, &index_count_);
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  built_radius_ = 0.f;
}

}  // namespace atmosphere
}  // namespace effect
