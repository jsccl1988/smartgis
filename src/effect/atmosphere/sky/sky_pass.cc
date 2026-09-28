// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/sky/sky_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "effect/atmosphere/detail/math.h"
#include "effect/atmosphere/detail/mesh.h"
#include "effect/atmosphere/detail/raster.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {

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

  const float elev = detail::clampf(p.sun_y, -1.0f, 1.0f);
  const float day = detail::clampf(elev * 1.5f + 0.2f, 0.0f, 1.0f);
  const float hr = detail::lerp(p.sunset_r, p.horizon_r, day);
  const float hg = detail::lerp(p.sunset_g, p.horizon_g, day);
  const float hb = detail::lerp(p.sunset_b, p.horizon_b, day);

  const float vy = detail::clampf(dy * 0.5f + 0.5f, 0.0f, 1.0f);
  const float blend = std::pow(vy, 0.55f);
  float r = detail::lerp(hr, p.zenith_r, blend);
  float g = detail::lerp(hg, p.zenith_g, blend);
  float b = detail::lerp(hb, p.zenith_b, blend);

  const float sun_dot =
      detail::clampf(dx * p.sun_x + dy * p.sun_y + dz * p.sun_z, 0.0f, 1.0f);
  const float glow = std::pow(sun_dot, 32.0f) * p.sun_glow_strength;
  r = detail::clampf(r + glow, 0.0f, 1.0f);
  g = detail::clampf(g + glow * 0.9f, 0.0f, 1.0f);
  b = detail::clampf(b + glow * 0.7f, 0.0f, 1.0f);

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
  pipeline_ = device->create_graphics_pipeline(
      render::programs::solid_pipeline_desc());
  return pipeline_ != nullptr;
}

bool SkyPass::ensure_dome_mesh(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (detail::static_mesh_ready(device_, vertex_, index_, device) &&
      index_count_ > 0) {
    return true;
  }

  // Coarse hemisphere (Y-up) large enough to sit behind orbit terrain.
  constexpr int kRings = 8;
  constexpr int kSegs = 16;
  const float R = params_.dome_radius > 0.5f ? params_.dome_radius : 8.0f;

  std::vector<float> verts;
  verts.reserve(static_cast<size_t>((kRings + 1) * (kSegs + 1) * 3));
  for (int ring = 0; ring <= kRings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(kRings);
    // 0 = zenith, 1 = horizon (slightly below to avoid a seam).
    const float phi = v * 1.65f;
    const float y = std::cos(phi) * R;
    const float rad = std::sin(phi) * R;
    for (int seg = 0; seg <= kSegs; ++seg) {
      const float u = static_cast<float>(seg) / static_cast<float>(kSegs);
      const float theta = u * 6.28318530718f;
      verts.push_back(std::cos(theta) * rad);
      verts.push_back(y);
      verts.push_back(std::sin(theta) * rad);
    }
  }

  std::vector<uint32_t> indices;
  indices.reserve(static_cast<size_t>(kRings * kSegs * 6));
  const int stride = kSegs + 1;
  for (int ring = 0; ring < kRings; ++ring) {
    for (int seg = 0; seg < kSegs; ++seg) {
      const uint32_t i0 = static_cast<uint32_t>(ring * stride + seg);
      const uint32_t i1 = i0 + 1;
      const uint32_t i2 = i0 + static_cast<uint32_t>(stride);
      const uint32_t i3 = i2 + 1;
      // Inward-facing so the camera at origin sees the inner surface.
      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i1);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }

  return detail::upload_static_mesh(
      &device_, &vertex_, &index_, &index_count_, device, verts.data(),
      static_cast<uint32_t>(verts.size() * sizeof(float)), indices.data(),
      static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
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

  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  average_sky_rgb(params_, &r, &g, &b);

  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  detail::apply_raster(
      list, {pipeline_, render::rhi::BlendMode::kOpaque, render::rhi::DepthMode::kWrite});
  // Slot 1 is the solid program's color float4.
  const float color[4] = {r, g, b, 1.0f};
  list->set_constants(1, color, static_cast<uint32_t>(sizeof(color)));
  detail::draw_indexed_mesh(list, vertex_, index_, 3 * sizeof(float),
                            index_count_);
  return true;
}

void SkyPass::release() {
  detail::release_static_mesh(&device_, &vertex_, &index_, &index_count_);
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

}  // namespace atmosphere
}  // namespace effect
