// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_SKY_PASS_H_
#define EFFECT_ATMOSPHERE_SKY_PASS_H_

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

namespace effect {
namespace atmosphere {

// POD knobs for a simple analytical sky (GIS 3D far-field).
struct SkyDrawParams {
  float sun_x = 0.0f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  // Orbit-normalized dome radius (matches Scene3dController framing).
  // Larger than max orbit distance (12) so the camera stays inside the dome.
  float dome_radius = 40.0f;
  float zenith_r = 0.08f;
  float zenith_g = 0.24f;
  float zenith_b = 0.82f;
  float horizon_r = 0.60f;
  float horizon_g = 0.73f;
  float horizon_b = 0.88f;
  float sunset_r = 0.90f;
  float sunset_g = 0.48f;
  float sunset_b = 0.30f;
  float sun_glow_strength = 0.50f;
};

// Far-sky / horizon tint driven by sun direction. Records a fullscreen NDC
// sky with a dedicated HLSL pipeline (CameraCB + SkyCB view-ray sample).
class SkyPass {
 public:
  SkyPass();
  ~SkyPass();

  SkyPass(const SkyPass&) = delete;
  SkyPass& operator=(const SkyPass&) = delete;

  void set_params(const SkyDrawParams& params);
  const SkyDrawParams& params() const { return params_; }

  void set_sun_direction(float x, float y, float z);
  void set_sun_from_azimuth_elevation(float azimuth_rad, float elevation_rad);

  // Sample RGB for a unit view direction (Y-up). Pure helper for tests / Null.
  static void sample_sky_rgb(const SkyDrawParams& p, float dir_x, float dir_y,
                             float dir_z, float* out_r, float* out_g,
                             float* out_b);

  // Average tint for tests and AtmosphereFrame clear color (zenith / horizon /
  // sun samples). Not used for the GPU dome draw.
  static void average_sky_rgb(const SkyDrawParams& p, float* out_r,
                              float* out_g, float* out_b);

  // Draw into an open CommandList (Frame owns begin/end_render_pass).
  bool record(render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
              uint32_t height, const render::rhi::CameraMatrices* camera);

  // Sky program created for the device passed to record. Null before that.
  render::rhi::Pipeline* pipeline() const { return pipeline_; }

  void release();

 private:
  bool ensure_dome_mesh(render::rhi::Device* device);
  bool ensure_pipeline(render::rhi::Device* device);
  void destroy_pipeline();

  SkyDrawParams params_;
  render::rhi::Device* device_ = nullptr;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Buffer* vertex_ = nullptr;
  render::rhi::Buffer* index_ = nullptr;
  uint32_t index_count_ = 0;
  float built_radius_ = 0.f;
};

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_SKY_PASS_H_
