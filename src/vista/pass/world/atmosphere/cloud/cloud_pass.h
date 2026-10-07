// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_CLOUD_PASS_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_CLOUD_PASS_H_

#include <cstdint>

namespace render {
namespace rhi {
class Buffer;
class CommandList;
class Device;
class Pipeline;
struct CameraMatrices;
}  // namespace rhi

}  // namespace render

#include "vista/vista_export.h"

namespace vista {

// Inputs for a single CPU reference ray through the cloud slab.
struct CloudRayInput {
  float origin_x = 0.0f;
  float origin_y = 0.0f;
  float origin_z = 0.0f;
  float dir_x = 0.0f;
  float dir_y = 1.0f;
  float dir_z = 0.0f;
  float sun_x = 0.0f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  float base_y = 1000.0f;
  float top_y = 3000.0f;
  float cover = 1.0f;
  float extinction = 0.02f;
  int steps = 16;
  // World-space density snap cell; 0 = full detail. Used as half-res proxy.
  float density_cell = 0.0f;
};

// Single-scatter + Beer integration result along one ray.
struct CloudRayResult {
  float luminance = 0.0f;
  float transmittance = 1.0f;
};

// Volumetric cloud raymarch modulated by cover and sun direction.
class VISTA_EXPORT CloudPass {
 public:
  CloudPass();
  ~CloudPass();

  CloudPass(const CloudPass&) = delete;
  CloudPass& operator=(const CloudPass&) = delete;

  // Sun direction (world space, need not be unit; normalized on set).
  void set_sun_direction(float x, float y, float z);
  void set_sun_from_azimuth_elevation(float azimuth_rad, float elevation_rad);

  void set_cloud_slab(float base_m, float top_m);
  void set_cover_modulation(float cover);
  void set_extinction(float sigma);

  // Orbit-normalized deck + slab (same frame as Scene3dPresenter terrain).
  // When orbit_slab_set_ is true, record() uses these instead of meter defaults.
  void set_deck_orbit(float half_x, float half_z, float y);
  void set_slab_orbit(float base_y, float top_y);

  // quality: raymarch step budget from AtmosphereParams::quality.
  // Opens a ColorLoadOp::kLoad pass only (never clears). GPU path uses the
  // cloud graphics pipeline with alpha blend and depth test (no write) so
  // DEM land stays landish; deck still raymarches over sky/ocean.
  // quality <= 1 uses a half-res *proxy* (fewer steps + coarser density snap);
  // no offscreen RT until RHI grows color attachments.
  bool record(render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
              uint32_t height, const render::rhi::CameraMatrices* camera, int quality);

  // Cloud program created for the device passed to record. Null before that.
  render::rhi::Pipeline* pipeline() const { return pipeline_; }

  void release();

  static int step_count_for_quality(int quality);
  // World snap cell for density; >0 when quality <= 1 (half-res proxy).
  static float density_cell_for_quality(int quality);
  static bool uses_half_res_proxy(int quality);

  static float beer_transmittance(float optical_depth);
  // Powder ≈ 1 - exp(-density * k); brightens thin media facing the light.
  static float powder_factor(float density);
  // Silver edge weight from view·sun (sun behind cloud when looking toward it).
  static float silver_lining(float dir_dot_sun);

  static float density_sample(float cover, float y, float base_y, float top_y,
                              float px, float py, float pz);
  static CloudRayResult march_ray(const CloudRayInput& in);

  float sun_x() const { return sun_x_; }
  float sun_y() const { return sun_y_; }
  float sun_z() const { return sun_z_; }
  float base_m() const { return base_m_; }
  float top_m() const { return top_m_; }
  float cover() const { return cover_; }

 private:
  bool ensure_deck_mesh(render::rhi::Device* device);
  bool ensure_pipeline(render::rhi::Device* device);
  void destroy_pipeline();

  float sun_x_ = 0.0f;
  float sun_y_ = 0.70710678f;
  float sun_z_ = 0.70710678f;
  float base_m_ = 1000.0f;
  float top_m_ = 3000.0f;
  float cover_ = 1.0f;
  float extinction_ = 0.02f;

  float deck_half_x_ = 2.0f;
  float deck_half_z_ = 2.0f;
  float deck_y_ = 0.85f;
  float orbit_base_y_ = 0.70f;
  float orbit_top_y_ = 1.10f;
  bool orbit_slab_set_ = false;
  bool deck_dirty_ = true;

  render::rhi::Device* device_ = nullptr;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Buffer* vertex_ = nullptr;
  render::rhi::Buffer* index_ = nullptr;
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_CLOUD_PASS_H_
