// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_ATMOSPHERE_GLOBE_SAT_CLOUD_PASS_H_
#define VISTA_PASS_ATMOSPHERE_GLOBE_SAT_CLOUD_PASS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace render {
namespace rhi {
class Buffer;
class CommandList;
class Device;
class Pipeline;
class Texture;
struct CameraMatrices;
}  // namespace rhi
}  // namespace render

#include "vista/vista_export.h"

namespace vista {

// Satellite cloud shell over the DEM globe (equirect cover, alpha blend).
struct SatCloudDrawParams {
  float shell_radius = 1.035f;  // Slightly above unit earth radius.
  int lon_slices = 96;
  int lat_slices = 48;
  float opacity = 0.70f;
  float soft_edge = 0.12f;
};

// Layer B: satellite / weather cloud map on a transparent sphere.
// Prefer real sat_cloud.tif when present; otherwise procedural cover stub.
class VISTA_EXPORT SatCloudPass {
 public:
  SatCloudPass();
  ~SatCloudPass();

  SatCloudPass(const SatCloudPass&) = delete;
  SatCloudPass& operator=(const SatCloudPass&) = delete;

  void set_params(const SatCloudDrawParams& params);
  const SatCloudDrawParams& params() const { return params_; }

  void set_time_sec(double t) { time_sec_ = t; }
  double time_sec() const { return time_sec_; }

  // RGBA8 equirect (alpha or luminance = cover). Owns a copy.
  void set_cover_rgba(const uint8_t* rgba, int w, int h);

  // Procedural banded / noise-like cover when no satellite sample exists.
  void seed_procedural_cover(int w = 256, int h = 128, uint32_t seed = 1);

  bool has_cover() const { return !cover_.empty(); }

  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height,
              const render::rhi::CameraMatrices* camera);

  void release();

 private:
  bool ensure_pipeline(render::rhi::Device* device);
  bool ensure_gpu(render::rhi::Device* device);
  void destroy_pipeline();
  void rebuild_mesh();

  SatCloudDrawParams params_;
  double time_sec_ = 0.0;
  bool mesh_dirty_ = true;
  bool cover_dirty_ = true;

  std::vector<uint8_t> cover_;
  int cover_w_ = 0;
  int cover_h_ = 0;

  std::vector<float> positions_;
  std::vector<uint32_t> indices_;

  render::rhi::Device* device_ = nullptr;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Buffer* vertex_buffer_ = nullptr;
  render::rhi::Buffer* index_buffer_ = nullptr;
  render::rhi::Texture* cover_tex_ = nullptr;
  uint32_t index_count_ = 0;
};

}  // namespace vista

#endif  // VISTA_PASS_ATMOSPHERE_GLOBE_SAT_CLOUD_PASS_H_
