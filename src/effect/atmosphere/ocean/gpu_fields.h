// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_OCEAN_GPU_FIELDS_H_
#define EFFECT_ATMOSPHERE_OCEAN_GPU_FIELDS_H_

#include <vector>

namespace render {
namespace rhi {
class CommandList;
class Device;
class Pipeline;
class Texture;
}  // namespace rhi

}  // namespace render

namespace effect {
namespace atmosphere {

struct OceanDrawParams;

namespace detail {

// Spectrum ping-pong and the encoded height map for one ocean pass.
// release() drops handles without destroy_* (FlyCube facade may be shut down).
class OceanGpuFields {
 public:
  OceanGpuFields() = default;
  ~OceanGpuFields();

  OceanGpuFields(const OceanGpuFields&) = delete;
  OceanGpuFields& operator=(const OceanGpuFields&) = delete;

  // owner_device is OceanPass::device_ (destroy target and mismatch check).
  // device is the Device argument of record().
  bool record(render::rhi::Device* owner_device, render::rhi::Device* device,
              render::rhi::CommandList* list, int n, const OceanDrawParams& params,
              double time_sec, float* height_scale, float* disp_scale);

  bool upload_height(render::rhi::Device* owner_device, render::rhi::Device* device, int n,
                     const std::vector<float>& heights,
                     const std::vector<float>& disp_x,
                     const std::vector<float>& disp_z, float height_scale,
                     float disp_scale);

  void destroy_height(render::rhi::Device* owner_device);
  void release();

  render::rhi::Texture* height() const { return height_; }

 private:
  bool ensure_textures(render::rhi::Device* owner_device, render::rhi::Device* device, int n);
  bool ensure_pipelines(render::rhi::Device* device);
  void destroy_pipelines();

  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* spectrum_ = nullptr;
  render::rhi::Pipeline* bit_reverse_ = nullptr;
  render::rhi::Pipeline* butterfly_ = nullptr;
  render::rhi::Pipeline* displace_ = nullptr;
  render::rhi::Pipeline* encode_ = nullptr;
  render::rhi::Pipeline* gaussian_h_ = nullptr;
  render::rhi::Pipeline* gaussian_v_ = nullptr;

  render::rhi::Texture* height_ = nullptr;
  // RGBA8 ping-pong scratch for separable Gaussian after height encode.
  render::rhi::Texture* blur_scratch_ = nullptr;
  render::rhi::Texture* spectrum_a_ = nullptr;
  render::rhi::Texture* spectrum_b_ = nullptr;
  render::rhi::Texture* spectrum_seed_ = nullptr;
  int height_n_ = 0;
  int spectrum_n_ = 0;
};

}  // namespace detail
}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_OCEAN_GPU_FIELDS_H_
