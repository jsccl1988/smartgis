// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_ATMOSPHERE_CLOUD_PASS_H_
#define RENDER_ATMOSPHERE_CLOUD_PASS_H_

#include <cstdint>

#include "render/render_export.h"

namespace render {
namespace rhi {
class Buffer;
class CommandList;
class Device;
struct CameraMatrices;
}  // namespace rhi

namespace atmosphere {

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
};

// Single-scatter + Beer integration result along one ray.
struct CloudRayResult {
  float luminance = 0.0f;
  float transmittance = 1.0f;
};

// Volumetric cloud raymarch modulated by cover and sun direction.
class RENDER_EXPORT CloudPass {
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

  // quality: raymarch step budget from AtmosphereParams::quality.
  // Opens a ColorLoadOp::kLoad pass only (never clears). GPU path uses
  // PipelineId::kCloud with alpha blend + depth test (no write).
  bool record(rhi::Device* device, rhi::CommandList* list, uint32_t width,
              uint32_t height, const rhi::CameraMatrices* camera, int quality);

  void release();

  static int step_count_for_quality(int quality);
  static float beer_transmittance(float optical_depth);
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
  bool ensure_deck_mesh(rhi::Device* device);

  float sun_x_ = 0.0f;
  float sun_y_ = 0.70710678f;
  float sun_z_ = 0.70710678f;
  float base_m_ = 1000.0f;
  float top_m_ = 3000.0f;
  float cover_ = 1.0f;
  float extinction_ = 0.02f;

  rhi::Device* device_ = nullptr;
  rhi::Buffer* vertex_ = nullptr;
  rhi::Buffer* index_ = nullptr;
};

}  // namespace atmosphere
}  // namespace render

#endif  // RENDER_ATMOSPHERE_CLOUD_PASS_H_
