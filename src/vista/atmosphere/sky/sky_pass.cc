// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/atmosphere/sky/sky_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "vista/atmosphere/detail/math.h"
#include "vista/atmosphere/detail/mesh.h"
#include "vista/atmosphere/detail/raster.h"
#include "vista/atmosphere/sky/constants.h"
#include "vista/atmosphere/sky/hlsl.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace {

constexpr uint32_t kSkyConstantSlot = 1;

render::rhi::GraphicsPipelineDesc sky_graphics_desc() {
  // Fullscreen NDC sky: SkyCB tint only (screen-space; no CameraCB — unused
  // buffers are stripped by DXC and break FlyCube GetBindKey).
  static constexpr render::rhi::BindingSlot kBindings[] = {
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
  desc.camera_slot = -1;
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
  (void)dir_x;
  (void)dir_z;
  // Match kPsSky: negative dome_radius → deep-space clear (globe splash).
  if (p.dome_radius < 0.f) {
    const float elev_v = detail::clampf(dir_y, 0.0f, 1.0f);
    const float r = detail::lerp(0.012f, 0.008f, elev_v);
    const float g = detail::lerp(0.014f, 0.010f, elev_v);
    const float b = detail::lerp(0.040f, 0.028f, elev_v);
    if (out_r) {
      *out_r = r;
    }
    if (out_g) {
      *out_g = g;
    }
    if (out_b) {
      *out_b = b;
    }
    return;
  }
  // Match kPsSky: screen-space Rayleigh only (dir_y stands in for elev_screen).
  // View-ray / Mie / sunset paths used to invent magenta mid-bands.
  const float elev_v = detail::clampf(dir_y, 0.0f, 1.0f);
  float zr = p.zenith_r;
  float zg = p.zenith_g;
  float zb = p.zenith_b;
  zg = (std::min)(zg, zb * 0.55f);
  zr = (std::min)(zr, zb * 0.35f);
  float hr = p.horizon_r;
  float hg = p.horizon_g;
  float hb = p.horizon_b;
  hr = (std::min)(hr, hb * 0.55f);
  hg = (std::min)(hg, detail::lerp(hg, hb, 0.30f));
  // Mild low-sun warm on the horizon sample only (CPU clear / tests) — keep
  // green so pink_frac gates never see G≈0 magenta.
  float sun_x = p.sun_x;
  float sun_y = p.sun_y;
  float sun_z = p.sun_z;
  detail::normalize3(&sun_x, &sun_y, &sun_z);
  const float elev = detail::clampf(sun_y, -1.0f, 1.0f);
  if (elev < 0.25f) {
    const float warm = detail::clampf(1.0f - elev * 4.0f, 0.0f, 0.35f);
    hr = detail::clampf(hr + warm * 0.12f, 0.0f, 1.0f);
    hg = detail::clampf(hg + warm * 0.04f, 0.0f, 1.0f);
  }
  const float blend =
      std::pow(detail::clampf(elev_v * 1.25f, 0.0f, 1.0f), 0.50f);
  float r = detail::lerp(hr, zr, blend);
  float g = detail::lerp(hg, zg, blend);
  float b = detail::lerp(hb, zb, blend);
  const float rayleigh = std::pow(elev_v, 0.55f);
  r *= detail::lerp(1.0f, 0.72f, rayleigh);
  g *= detail::lerp(1.0f, 0.88f, rayleigh);
  b *= detail::lerp(1.0f, 1.22f, rayleigh);
  r = detail::clampf(r, 0.0f, 1.0f);
  g = detail::clampf(g, 0.0f, 1.0f);
  b = detail::clampf(b, 0.0f, 1.0f);

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
  // Match release(): FlyCube Device may already be shut down / recycled.
  // A dangling or freefill pipeline_device_ AVs on the vtable load
  // (atmosphere-showcase full @ present-warm → SkyPass::destroy_pipeline).
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
  // Abandon stale handles — never virtual-call destroy on a mismatched or
  // corrupted pipeline_device_ (same policy as release()).
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(sky_graphics_desc());
  if (!pipeline_) {
    pipeline_device_ = nullptr;
    return false;
  }
  return true;
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
  sky.space_blend = (params_.dome_radius < 0.f) ? 1.0f : 0.0f;
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
  // Screen-space sky ignores the orbit camera (camera_slot = -1).
  (void)camera;
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

}  // namespace vista
