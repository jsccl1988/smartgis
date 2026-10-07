// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/atmosphere/cloud/cloud_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "vista/pass/world/atmosphere/cloud/constants.h"
#include "vista/pass/world/atmosphere/cloud/hlsl.h"
#include "vista/component/world/atmosphere/detail/math.h"
#include "vista/pass/world/atmosphere/detail/mesh.h"
#include "vista/pass/world/atmosphere/detail/raster.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace {

constexpr float kInvFourPi = 1.0f / (4.0f * 3.14159265358979323846f);
constexpr uint32_t kCloudConstantSlot = 1;
// Matches kPsCloud powder / silver constants.
constexpr float kPowderScale = 8.0f;
constexpr float kSilverBoost = 1.5f;
// Coarse world snap for quality <= 1 (half-res proxy; no offscreen RT).
// Orbit-normalized China frame spans ~±2. A 64-unit snap collapsed every
// sample to the origin and made the deck look empty at quality ≤ 1.
constexpr float kHalfResDensityCell = 0.22f;

float snap_axis(float v, float cell) {
  if (cell <= 0.0f) {
    return v;
  }
  return std::floor(v / cell + 0.5f) * cell;
}

render::rhi::GraphicsPipelineDesc cloud_graphics_desc() {
  static constexpr render::rhi::BindingSlot kBindings[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = 128,
       .hlsl_name = "CameraCB"},
      {.slot = kCloudConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = sizeof(CloudConstants),
       .hlsl_name = "CloudCB"},
  };
  render::rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = kVsCloud;
  desc.pixel.hlsl = kPsCloud;
  desc.vertex_layout = render::rhi::VertexLayout::kPosition;
  desc.bindings = kBindings;
  desc.binding_count = sizeof(kBindings) / sizeof(kBindings[0]);
  desc.blend = render::rhi::BlendMode::kSrcAlpha;
  desc.compile_depth_off = false;
  desc.compile_depth_write = false;
  desc.compile_depth_test = true;
  desc.camera_slot = 0;
  return desc;
}

// Cheap 3D hash noise in [0,1] for density modulation.
float noise3(float x, float y, float z) {
  const int ix = static_cast<int>(std::floor(x));
  const int iy = static_cast<int>(std::floor(y));
  const int iz = static_cast<int>(std::floor(z));
  auto hash = [](int a, int b, int c) -> float {
    uint32_t n = static_cast<uint32_t>(a) * 374761393u +
                 static_cast<uint32_t>(b) * 668265263u +
                 static_cast<uint32_t>(c) * 2246822519u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    return static_cast<float>((n ^ (n >> 16u)) & 0x00FFFFFFu) /
           static_cast<float>(0x01000000u);
  };
  const float fx = x - static_cast<float>(ix);
  const float fy = y - static_cast<float>(iy);
  const float fz = z - static_cast<float>(iz);
  const float sx = fx * fx * (3.0f - 2.0f * fx);
  const float sy = fy * fy * (3.0f - 2.0f * fy);
  const float sz = fz * fz * (3.0f - 2.0f * fz);
  const float n000 = hash(ix, iy, iz);
  const float n100 = hash(ix + 1, iy, iz);
  const float n010 = hash(ix, iy + 1, iz);
  const float n110 = hash(ix + 1, iy + 1, iz);
  const float n001 = hash(ix, iy, iz + 1);
  const float n101 = hash(ix + 1, iy, iz + 1);
  const float n011 = hash(ix, iy + 1, iz + 1);
  const float n111 = hash(ix + 1, iy + 1, iz + 1);
  const float x00 = n000 + (n100 - n000) * sx;
  const float x10 = n010 + (n110 - n010) * sx;
  const float x01 = n001 + (n101 - n001) * sx;
  const float x11 = n011 + (n111 - n011) * sx;
  const float y0 = x00 + (x10 - x00) * sy;
  const float y1 = x01 + (x11 - x01) * sy;
  return y0 + (y1 - y0) * sz;
}

// Ray vs Y-slab [base, top]. Returns false if no hit.
bool intersect_slab_y(float oy, float dy, float base_y, float top_y, float* t0,
                      float* t1) {
  if (!t0 || !t1) {
    return false;
  }
  float lo = base_y;
  float hi = top_y;
  if (hi < lo) {
    std::swap(lo, hi);
  }
  if (std::fabs(dy) < 1.0e-6f) {
    if (oy < lo || oy > hi) {
      return false;
    }
    *t0 = 0.0f;
    *t1 = 1.0e6f;
    return true;
  }
  float ta = (lo - oy) / dy;
  float tb = (hi - oy) / dy;
  if (ta > tb) {
    std::swap(ta, tb);
  }
  *t0 = (std::max)(ta, 0.0f);
  *t1 = tb;
  return *t1 > *t0;
}

}  // namespace

CloudPass::CloudPass() = default;

CloudPass::~CloudPass() {
  // FlyCube Device may already be shut down (Scene3dPresenter teardown).
  // Abandon pipeline handles; do not destroy_* on a dangling Device*.
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  release();
}

void CloudPass::destroy_pipeline() {
  // Abandon only — see SkyPass::destroy_pipeline (dangling Device* AV).
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool CloudPass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_ && pipeline_device_ == device) {
    return true;
  }
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(cloud_graphics_desc());
  if (!pipeline_) {
    pipeline_device_ = nullptr;
    return false;
  }
  return true;
}

void CloudPass::set_sun_direction(float x, float y, float z) {
  sun_x_ = x;
  sun_y_ = y;
  sun_z_ = z;
  detail::normalize3(&sun_x_, &sun_y_, &sun_z_);
}

void CloudPass::set_sun_from_azimuth_elevation(float azimuth_rad,
                                               float elevation_rad) {
  detail::sun_from_azimuth_elevation(azimuth_rad, elevation_rad, &sun_x_,
                                     &sun_y_, &sun_z_);
}

void CloudPass::set_cloud_slab(float base_m, float top_m) {
  base_m_ = base_m;
  top_m_ = top_m;
  if (top_m_ < base_m_) {
    std::swap(base_m_, top_m_);
  }
}

void CloudPass::set_deck_orbit(float half_x, float half_z, float y) {
  const float hx = std::max(0.01f, half_x);
  const float hz = std::max(0.01f, half_z);
  if (std::fabs(deck_half_x_ - hx) > 1.0e-5f ||
      std::fabs(deck_half_z_ - hz) > 1.0e-5f ||
      std::fabs(deck_y_ - y) > 1.0e-5f) {
    deck_dirty_ = true;
  }
  deck_half_x_ = hx;
  deck_half_z_ = hz;
  deck_y_ = y;
}

void CloudPass::set_slab_orbit(float base_y, float top_y) {
  orbit_base_y_ = base_y;
  orbit_top_y_ = top_y;
  if (orbit_top_y_ < orbit_base_y_) {
    std::swap(orbit_base_y_, orbit_top_y_);
  }
  orbit_slab_set_ = true;
}

void CloudPass::set_cover_modulation(float cover) {
  cover_ = detail::clampf(cover, 0.0f, 1.0f);
}

void CloudPass::set_extinction(float sigma) {
  extinction_ = std::max(0.0f, sigma);
}

int CloudPass::step_count_for_quality(int quality) {
  const int q = std::max(0, std::min(3, quality));
  return 8 << q;
}

bool CloudPass::uses_half_res_proxy(int quality) {
  return quality <= 1;
}

float CloudPass::density_cell_for_quality(int quality) {
  // Half-res proxy: snap density samples to a coarser grid until RHI can
  // host a true half-res color attachment + upsample blit.
  return uses_half_res_proxy(quality) ? kHalfResDensityCell : 0.0f;
}

float CloudPass::beer_transmittance(float optical_depth) {
  return std::exp(-std::max(0.0f, optical_depth));
}

float CloudPass::powder_factor(float density) {
  const float dens = std::max(0.0f, density);
  return 1.0f - std::exp(-dens * kPowderScale);
}

float CloudPass::silver_lining(float dir_dot_sun) {
  const float toward = detail::clampf(dir_dot_sun, 0.0f, 1.0f);
  return toward * toward;
}

float CloudPass::density_sample(float cover, float y, float base_y, float top_y,
                                float px, float py, float pz) {
  const float c = detail::clampf(cover, 0.0f, 1.0f);
  if (c <= 0.0f) {
    return 0.0f;
  }
  float lo = base_y;
  float hi = top_y;
  if (hi < lo) {
    std::swap(lo, hi);
  }
  const float thickness = hi - lo;
  if (thickness <= 1.0e-3f || y < lo || y > hi) {
    return 0.0f;
  }
  const float t = (y - lo) / thickness;
  const float falloff = 4.0f * t * (1.0f - t);
  // Cover texture stand-in: 3D noise at world position (scale ~ km).
  const float n = noise3(px * 0.001f, py * 0.001f, pz * 0.001f);
  return c * falloff * (0.35f + 0.65f * n);
}

CloudRayResult CloudPass::march_ray(const CloudRayInput& in) {
  CloudRayResult out;
  float dx = in.dir_x;
  float dy = in.dir_y;
  float dz = in.dir_z;
  detail::normalize3(&dx, &dy, &dz);

  float sun_x = in.sun_x;
  float sun_y = in.sun_y;
  float sun_z = in.sun_z;
  detail::normalize3(&sun_x, &sun_y, &sun_z);

  float t_enter = 0.0f;
  float t_exit = 0.0f;
  if (!intersect_slab_y(in.origin_y, dy, in.base_y, in.top_y, &t_enter,
                        &t_exit)) {
    return out;
  }

  const int steps = std::max(1, in.steps);
  const float span = t_exit - t_enter;
  const float ds = span / static_cast<float>(steps);
  float T = 1.0f;
  float L = 0.0f;
  const float sigma_scale = std::max(0.0f, in.extinction);
  const float cover = detail::clampf(in.cover, 0.0f, 1.0f);
  const float cell = std::max(0.0f, in.density_cell);
  // Silver lining: sun behind the cloud when the view looks toward the sun.
  const float dir_dot_sun = dx * sun_x + dy * sun_y + dz * sun_z;
  const float silver = silver_lining(dir_dot_sun);

  for (int i = 0; i < steps; ++i) {
    const float t = t_enter + (static_cast<float>(i) + 0.5f) * ds;
    float px = snap_axis(in.origin_x + dx * t, cell);
    float py = snap_axis(in.origin_y + dy * t, cell);
    float pz = snap_axis(in.origin_z + dz * t, cell);
    const float density =
        density_sample(cover, py, in.base_y, in.top_y, px, py, pz);
    if (density <= 0.0f) {
      continue;
    }
    const float sigma = density * sigma_scale;
    const float optical = sigma * ds;
    // Beer along the view ray (unchanged by powder / silver).
    const float step_T = beer_transmittance(optical);
    // Single scatter: isotropic phase * remaining transmittance.
    // Cheap sun shadow: one sample toward the sun through remaining slab.
    float shadow_T = 1.0f;
    {
      float st0 = 0.0f;
      float st1 = 0.0f;
      if (intersect_slab_y(py, sun_y, in.base_y, in.top_y, &st0, &st1) &&
          st1 > 0.0f) {
        const float shadow_ds = std::min(st1, span) * 0.25f;
        const float spx = snap_axis(px + sun_x * shadow_ds, cell);
        const float spy = snap_axis(py + sun_y * shadow_ds, cell);
        const float spz = snap_axis(pz + sun_z * shadow_ds, cell);
        const float sd =
            density_sample(cover, spy, in.base_y, in.top_y, spx, spy, spz);
        shadow_T = beer_transmittance(sd * sigma_scale * shadow_ds);
      }
    }
    const float powder = powder_factor(density);
    const float scatter = powder * (1.0f + silver * kSilverBoost);
    L += T * (1.0f - step_T) * shadow_T * kInvFourPi * scatter;
    T *= step_T;
    if (T < 1.0e-3f) {
      break;
    }
  }

  out.luminance = L;
  out.transmittance = T;
  return out;
}

bool CloudPass::ensure_deck_mesh(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (!deck_dirty_ &&
      detail::static_mesh_ready(device_, vertex_, index_, device)) {
    return true;
  }
  // Rectangular XZ deck in orbit-normalized space (aligned with China extent).
  const float hx = deck_half_x_;
  const float hz = deck_half_z_;
  const float y = deck_y_;
  const float xyz[12] = {-hx, y, -hz, hx, y, -hz, hx, y, hz, -hx, y, hz};
  const uint32_t indices[6] = {0, 1, 2, 0, 2, 3};
  if (!detail::upload_static_mesh(
          &device_, &vertex_, &index_, nullptr, device, xyz,
          static_cast<uint32_t>(sizeof(xyz)), indices,
          static_cast<uint32_t>(sizeof(indices)))) {
    return false;
  }
  deck_dirty_ = false;
  return true;
}

bool CloudPass::record(render::rhi::Device* device, render::rhi::CommandList* list,
                       uint32_t width, uint32_t height,
                       const render::rhi::CameraMatrices* camera, int quality) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!ensure_deck_mesh(device)) {
    return false;
  }
  if (!ensure_pipeline(device)) {
    return false;
  }

  // Drive CPU reference once so quality / sun / cover stay exercised on Null.
  CloudRayInput ray;
  ray.origin_x = 0.0f;
  ray.origin_y = 0.0f;
  ray.origin_z = 0.0f;
  ray.dir_x = 0.0f;
  ray.dir_y = 1.0f;
  ray.dir_z = 0.0f;
  ray.sun_x = sun_x_;
  ray.sun_y = sun_y_;
  ray.sun_z = sun_z_;
  ray.base_y = base_m_;
  ray.top_y = top_m_;
  ray.cover = cover_;
  ray.extinction = extinction_;
  ray.steps = step_count_for_quality(quality);
  ray.density_cell = density_cell_for_quality(quality);
  const CloudRayResult baked = march_ray(ray);
  (void)baked;

  CloudConstants cloud{};
  cloud.sun_x = sun_x_;
  cloud.sun_y = sun_y_;
  cloud.sun_z = sun_z_;
  cloud.cover = cover_;
  // Prefer host-projected orbit slab (China 3D frame); else legacy defaults.
  if (orbit_slab_set_) {
    cloud.base_m = orbit_base_y_;
    cloud.top_m = orbit_top_y_;
  } else {
    cloud.base_m = 0.70f;
    cloud.top_m = 1.10f;
  }
  // Orbit slab is thin (~0.2). Keep extinction moderate: too high (×72)
  // saturated Beer to black and painted the whole DEM under the deck.
  cloud.extinction = extinction_ * 28.0f;
  cloud.steps = static_cast<float>(step_count_for_quality(quality));
  cloud.density_cell = density_cell_for_quality(quality);
  if (camera) {
    detail::eye_from_view(camera->view, &cloud.cam_x, &cloud.cam_y, &cloud.cam_z);
  }

  detail::set_fullscreen_viewport(list, width, height);
  // Load-only marker: never clear; prior ocean/land color is preserved.
  detail::begin_load_pass(list, width, height);
  detail::bind_camera_if(list, camera);
  // TestOnly: composite over sky/ocean depth, but do not paint a veil over DEM
  // (Disabled + strong extinction previously crushed landish to black).
  detail::apply_raster(list, {pipeline_, render::rhi::BlendMode::kSrcAlpha,
                              render::rhi::DepthMode::kTestOnly});
  list->set_constants(kCloudConstantSlot, &cloud,
                      static_cast<uint32_t>(sizeof(cloud)));
  detail::draw_indexed_mesh(list, vertex_, index_, 3 * sizeof(float), 6);
  list->end_render_pass();
  return true;
}

void CloudPass::release() {
  // Same FlyCube shutdown policy as OceanPass::release ? do not destroy_* on a
  // facade that may already have been shut down / leaked.
  detail::release_static_mesh(&device_, &vertex_, &index_, nullptr);
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  deck_dirty_ = true;
}

}  // namespace vista
