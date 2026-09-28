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
struct CameraMatrices;
}  // namespace rhi

}  // namespace render

namespace effect {
namespace atmosphere {

// Height / distance fog knobs (GIS visibility). Depth attaches via shared RT.
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
  float max_opacity = 0.55f;
};

// Post-opaque haze: dedicated fog HLSL on a fullscreen NDC triangle (SrcAlpha).
// Depth sampling against the shared RT remains deferred — this pass soft-washes
// the frame without a depth test.
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
  bool record(render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
              uint32_t height, const render::rhi::CameraMatrices* camera);

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
