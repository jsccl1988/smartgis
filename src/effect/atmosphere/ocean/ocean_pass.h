// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_OCEAN_PASS_H_
#define EFFECT_ATMOSPHERE_OCEAN_PASS_H_

#include <cstddef>
#include <cstdint>
#include <vector>


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

class FieldTexture;

namespace detail {
class OceanGpuFields;
}  // namespace detail

// CPU/GPU ocean draw knobs (mirrors gis::atmosphere::OceanSpectrumParams).
struct OceanDrawParams {
  float significant_wave_height = 1.0f;
  float mean_direction_rad = 0.0f;
  float wind_speed = 5.0f;
  float wind_direction_rad = 0.0f;
  int fft_size = 64;
  bool use_gerstner_fallback = false;
  // Prefer FlyCube compute FFT when Device::supports_compute(); Null falls back.
  bool prefer_gpu_fft = true;
  // Peak-enhanced directional JONSWAP-lite; false → Phillips fallback.
  bool use_jonswap = true;
  // Tessendorf chop strength for horizontal displacement.
  float chop = 1.0f;
  float jonswap_gamma = 3.3f;

  // Orbit / world patch half-extent on XZ when patch_half_x/z are 0.
  float patch_half_extent = 50.0f;
  // Optional rectangular patch in the same space as Scene3d orbit mesh
  // (X=-lon normalized). Zero half_x/z → fall back to patch_half_extent.
  float patch_center_x = 0.0f;
  float patch_center_z = 0.0f;
  float patch_y = 0.0f;
  float patch_half_x = 0.0f;
  float patch_half_z = 0.0f;
  // Displaceable mesh resolution (vertices per edge); independent of FFT size.
  int mesh_resolution = 33;

  float deep_r = 0.02f;
  float deep_g = 0.12f;
  float deep_b = 0.28f;
  float deep_a = 1.0f;
  float shallow_r = 0.15f;
  float shallow_g = 0.45f;
  float shallow_b = 0.55f;
  float shallow_a = 1.0f;
  float fresnel_bias = 0.04f;
  float fresnel_power = 5.0f;
  float sea_mask_threshold = 0.5f;
};

// GPU ocean: JONSWAP/Phillips spectrum + radix-2 FFT → height + Dx/Dz; VS
// displaces with chop. CPU FFT/Gerstner remain as fallback.
class OceanPass {
 public:
  OceanPass();
  ~OceanPass();

  OceanPass(const OceanPass&) = delete;
  OceanPass& operator=(const OceanPass&) = delete;

  void set_params(const OceanDrawParams& params);
  const OceanDrawParams& params() const { return params_; }

  void set_time_sec(double t) { time_sec_ = t; }
  double time_sec() const { return time_sec_; }

  // Optional sea mask (1 = sea). CPU path discards land triangles.
  void set_sea_mask_texture(FieldTexture* mask);
  void set_sea_mask_cpu(int cols, int rows, const float* values,
                        std::size_t value_count);

  // True after the last successful record() that queued GPU FFT dispatches.
  bool used_gpu_fft() const { return used_gpu_fft_; }

  // Encode scales after the last record (Hs energy-normalized for FFT paths).
  float height_scale() const { return height_scale_; }
  float disp_scale() const { return disp_scale_; }

  // Record into an open CommandList (same Device as GpuScene). Does not close.
  bool record(render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
              uint32_t height, const render::rhi::CameraMatrices* camera);

  // Ocean graphics program created for the device passed to record.
  render::rhi::Pipeline* pipeline() const { return pipeline_; }

  void release();

 private:
  bool ensure_resources(render::rhi::Device* device);
  bool ensure_pipeline(render::rhi::Device* device);
  void destroy_pipeline();
  void rebuild_mesh_grid();
  void rebuild_displacement();
  void rebuild_indices_with_mask();
  float sample_sea_mask(float u, float v) const;

  OceanDrawParams params_;
  double time_sec_ = 0.0;
  bool used_gpu_fft_ = false;

  render::rhi::Device* device_ = nullptr;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Buffer* vertex_buffer_ = nullptr;
  render::rhi::Buffer* index_buffer_ = nullptr;
  detail::OceanGpuFields* gpu_ = nullptr;
  uint32_t index_count_ = 0;
  uint32_t vertex_count_ = 0;
  int cached_mesh_res_ = 0;

  // Interleaved xyz + uv (5 floats) for the ocean graphics pipeline.
  std::vector<float> positions_;
  std::vector<float> heights_;  // mesh_n * mesh_n raw heights (CPU path)
  std::vector<float> disp_x_;   // CPU path horizontal displacement
  std::vector<float> disp_z_;
  std::vector<uint32_t> indices_;
  float height_scale_ = 1.0f;
  float disp_scale_ = 1.0f;

  FieldTexture* sea_mask_tex_ = nullptr;
  int mask_cols_ = 0;
  int mask_rows_ = 0;
  std::vector<float> mask_cpu_;
};

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_OCEAN_PASS_H_
