// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_FOG_PASS_H_
#define EFFECT_ATMOSPHERE_FOG_PASS_H_

#include <cstdint>


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

namespace effect {
namespace atmosphere {

// Height / distance fog knobs (GIS visibility). Depth SRV is optional.
struct FogDrawParams {
  // Tuned for China orbit (~3.2 camera span).
  float density = 0.08f;
  // Soft falloff above this world Y (orbit-normalized units).
  float height_falloff = 1.2f;
  float base_height = 0.0f;
  // Characteristic distance for exponential fog (same units as camera).
  float visibility = 4.0f;
  float color_r = 0.70f;
  float color_g = 0.76f;
  float color_b = 0.84f;
  float max_opacity = 0.65f;
};

// Post-opaque haze: dedicated fog HLSL on a fullscreen NDC triangle (SrcAlpha).
// Optional scene-depth SRV reconstructs aerial distance; without it the PS
// falls back to a CameraCB far-ray (Device depth wiring is parent-owned).
class FogPass {
 public:
  FogPass();
  ~FogPass();

  FogPass(const FogPass&) = delete;
  FogPass& operator=(const FogPass&) = delete;

  void set_params(const FogDrawParams& params);
  const FogDrawParams& params() const { return params_; }

  // Exponential distance × height attenuation in [0, 1] (CPU mirror of kPsFog).
  static float fog_factor(const FogDrawParams& p, float distance,
                         float height_y);

  // Opens a ColorLoadOp::kLoad pass (never clears). Does not close the list.
  // |depth| non-null binds slot 0 and enables the depth-sample path.
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height,
              const render::rhi::CameraMatrices* camera,
              render::rhi::Texture* depth = nullptr);

  // Fog graphics program created for the device passed to record. Null before.
  render::rhi::Pipeline* pipeline() const { return pipeline_; }

  void release();

 private:
  bool ensure_fullscreen_mesh(render::rhi::Device* device);
  bool ensure_pipeline(render::rhi::Device* device);
  void destroy_pipeline();

  FogDrawParams params_;
  render::rhi::Device* device_ = nullptr;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Buffer* vertex_ = nullptr;
  render::rhi::Buffer* index_ = nullptr;
};

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_FOG_PASS_H_
