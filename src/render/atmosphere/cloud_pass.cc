// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/atmosphere/cloud_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "render/rhi/rhi.h"

namespace render {
namespace atmosphere {
namespace {

constexpr float kInvFourPi = 1.0f / (4.0f * 3.14159265358979323846f);

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

void normalize3(float* x, float* y, float* z) {
  const float len = std::sqrt((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
  if (len > 1.0e-6f) {
    *x /= len;
    *y /= len;
    *z /= len;
  } else {
    *x = 0.0f;
    *y = 1.0f;
    *z = 0.0f;
  }
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
  *t0 = std::max(ta, 0.0f);
  *t1 = tb;
  return *t1 > *t0;
}

}  // namespace

CloudPass::CloudPass() = default;

CloudPass::~CloudPass() {
  release();
}

void CloudPass::set_sun_direction(float x, float y, float z) {
  sun_x_ = x;
  sun_y_ = y;
  sun_z_ = z;
  normalize3(&sun_x_, &sun_y_, &sun_z_);
}

void CloudPass::set_sun_from_azimuth_elevation(float azimuth_rad,
                                               float elevation_rad) {
  const float ce = std::cos(elevation_rad);
  sun_x_ = std::sin(azimuth_rad) * ce;
  sun_y_ = std::sin(elevation_rad);
  sun_z_ = std::cos(azimuth_rad) * ce;
  normalize3(&sun_x_, &sun_y_, &sun_z_);
}

void CloudPass::set_cloud_slab(float base_m, float top_m) {
  base_m_ = base_m;
  top_m_ = top_m;
  if (top_m_ < base_m_) {
    std::swap(base_m_, top_m_);
  }
}

void CloudPass::set_cover_modulation(float cover) {
  cover_ = clampf(cover, 0.0f, 1.0f);
}

void CloudPass::set_extinction(float sigma) {
  extinction_ = std::max(0.0f, sigma);
}

int CloudPass::step_count_for_quality(int quality) {
  const int q = std::max(0, std::min(3, quality));
  return 8 << q;
}

float CloudPass::beer_transmittance(float optical_depth) {
  return std::exp(-std::max(0.0f, optical_depth));
}

float CloudPass::density_sample(float cover, float y, float base_y, float top_y,
                                float px, float py, float pz) {
  const float c = clampf(cover, 0.0f, 1.0f);
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
  normalize3(&dx, &dy, &dz);

  float sun_x = in.sun_x;
  float sun_y = in.sun_y;
  float sun_z = in.sun_z;
  normalize3(&sun_x, &sun_y, &sun_z);

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
  const float cover = clampf(in.cover, 0.0f, 1.0f);

  for (int i = 0; i < steps; ++i) {
    const float t = t_enter + (static_cast<float>(i) + 0.5f) * ds;
    const float px = in.origin_x + dx * t;
    const float py = in.origin_y + dy * t;
    const float pz = in.origin_z + dz * t;
    const float density =
        density_sample(cover, py, in.base_y, in.top_y, px, py, pz);
    if (density <= 0.0f) {
      continue;
    }
    const float sigma = density * sigma_scale;
    const float optical = sigma * ds;
    // Beer along the view ray.
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
        const float spx = px + sun_x * shadow_ds;
        const float spy = py + sun_y * shadow_ds;
        const float spz = pz + sun_z * shadow_ds;
        const float sd =
            density_sample(cover, spy, in.base_y, in.top_y, spx, spy, spz);
        shadow_T = beer_transmittance(sd * sigma_scale * shadow_ds);
      }
    }
    L += T * (1.0f - step_T) * shadow_T * kInvFourPi;
    T *= step_T;
    if (T < 1.0e-3f) {
      break;
    }
  }

  out.luminance = L;
  out.transmittance = T;
  return out;
}

bool CloudPass::ensure_deck_mesh(rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (device_ == device && vertex_ && index_) {
    return true;
  }
  release();
  device_ = device;

  // Horizontal deck in orbit-normalized space (matches Scene3dController ocean
  // framing). Alpha-blended cloud pipeline raymarches through the slab.
  constexpr float kY = 0.85f;
  constexpr float kE = 2.0f;
  const float verts[] = {
      -kE, kY, -kE, kE, kY, -kE, kE, kY, kE, -kE, kY, kE,
  };
  const uint32_t indices[] = {0, 1, 2, 0, 2, 3};

  vertex_ = device->create_buffer(static_cast<uint32_t>(sizeof(verts)),
                                  rhi::BufferUsage::kVertex);
  index_ = device->create_buffer(static_cast<uint32_t>(sizeof(indices)),
                                 rhi::BufferUsage::kIndex);
  if (!vertex_ || !index_) {
    release();
    return false;
  }
  if (!device->upload(vertex_, verts, static_cast<uint32_t>(sizeof(verts))) ||
      !device->upload(index_, indices, static_cast<uint32_t>(sizeof(indices)))) {
    release();
    return false;
  }
  return true;
}

bool CloudPass::record(rhi::Device* device, rhi::CommandList* list,
                       uint32_t width, uint32_t height,
                       const rhi::CameraMatrices* camera, int quality) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!ensure_deck_mesh(device)) {
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
  const CloudRayResult baked = march_ray(ray);

  rhi::CloudGpuParams cloud;
  cloud.sun_x = sun_x_;
  cloud.sun_y = sun_y_;
  cloud.sun_z = sun_z_;
  cloud.cover = cover_;
  // Deck lives in orbit-normalized space (~unit); map slab meters into that
  // frame so the GPU raymarch intersects the drawn deck.
  cloud.base_m = 0.70f;
  cloud.top_m = 1.10f;
  cloud.extinction = extinction_ * 40.0f;
  cloud.steps = static_cast<float>(step_count_for_quality(quality));
  if (camera) {
    cloud.cam_x = -camera->view[12];
    cloud.cam_y = -camera->view[13];
    cloud.cam_z = -camera->view[14];
  }

  list->set_viewport(0.f, 0.f, static_cast<float>(width),
                     static_cast<float>(height), 0.f, 1.f);
  // Load-only marker: never clear; prior ocean/land color is preserved.
  rhi::RenderPassDesc pass;
  pass.width = width;
  pass.height = height;
  pass.load_op = rhi::ColorLoadOp::kLoad;
  pass.enable_depth = true;
  pass.depth_load_op = rhi::DepthLoadOp::kLoad;
  list->begin_render_pass(pass);
  if (camera) {
    list->bind_camera(*camera);
  }
  list->set_pipeline(rhi::PipelineId::kCloud);
  list->set_blend_mode(rhi::BlendMode::kSrcAlpha);
  list->set_depth_mode(rhi::DepthMode::kTestOnly);
  list->set_cloud_params(cloud);
  const float q_norm =
      static_cast<float>(step_count_for_quality(quality)) / 64.0f;
  const float lum = clampf(baked.luminance * 8.0f, 0.0f, 1.0f);
  // Alpha encodes cover / transmittance for Null inspection.
  list->set_solid_color(0.85f * lum + 0.15f * cover_,
                        0.88f * lum + 0.12f * cover_,
                        0.95f * lum + 0.05f * (1.0f - cover_),
                        clampf(1.0f - baked.transmittance + 0.15f * q_norm, 0.0f,
                               1.0f));
  list->bind_vertex_buffer(vertex_, 0, 3 * sizeof(float));
  list->bind_index_buffer(index_, 0);
  list->draw_indexed(6, 1, 0, 0, 0);
  list->end_render_pass();
  return true;
}

void CloudPass::release() {
  if (device_) {
    if (vertex_) {
      device_->destroy_buffer(vertex_);
      vertex_ = nullptr;
    }
    if (index_) {
      device_->destroy_buffer(index_);
      index_ = nullptr;
    }
  }
  device_ = nullptr;
}

}  // namespace atmosphere
}  // namespace render
