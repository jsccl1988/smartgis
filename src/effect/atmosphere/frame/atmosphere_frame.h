// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_ATMOSPHERE_FRAME_H_
#define EFFECT_ATMOSPHERE_ATMOSPHERE_FRAME_H_

#include <cstdint>


namespace render {
namespace rhi {
class CommandList;
class Device;
struct CameraMatrices;
}  // namespace rhi

}  // namespace render

namespace effect {
namespace atmosphere {

class CloudPass;
class FogPass;
class OceanPass;
class SkyPass;

// Owns pass-order policy for environment draws around opaque geometry.
// Contract: sky? → ocean → (host GpuScene opaque) → cloud → fog?
class AtmosphereFrame {
 public:
  AtmosphereFrame() = default;
  ~AtmosphereFrame() = default;

  AtmosphereFrame(const AtmosphereFrame&) = delete;
  AtmosphereFrame& operator=(const AtmosphereFrame&) = delete;

  void set_ocean_pass(OceanPass* pass) { ocean_pass_ = pass; }
  void set_cloud_pass(CloudPass* pass) { cloud_pass_ = pass; }
  void set_sky_pass(SkyPass* pass) { sky_pass_ = pass; }
  void set_fog_pass(FogPass* pass) { fog_pass_ = pass; }

  // Mirrors session toggles projected from gis::AtmosphereParams.
  void set_ocean_enabled(bool on) { ocean_enabled_ = on; }
  void set_cloud_enabled(bool on) { cloud_enabled_ = on; }
  void set_sky_enabled(bool on) { sky_enabled_ = on; }
  void set_fog_enabled(bool on) { fog_enabled_ = on; }

  bool ocean_enabled() const { return ocean_enabled_; }
  bool cloud_enabled() const { return cloud_enabled_; }
  bool sky_enabled() const { return sky_enabled_; }
  bool fog_enabled() const { return fog_enabled_; }

  // True if pre-opaque pass will clear color (ocean and/or sky).
  bool clears_color() const;
  // True when opaque GpuScene should share depth with atmosphere passes.
  bool uses_shared_depth() const;

  // sky + ocean. Does not close the CommandList.
  // ocean/sky enabled && matching pass nullptr → false (fail closed).
  // disabled → true no-op.
  bool record_pre_opaque(render::rhi::Device* device, render::rhi::CommandList* list,
                         uint32_t width, uint32_t height,
                         const render::rhi::CameraMatrices* camera);

  // clouds + fog. Call after GpuScene::record_draws.
  // cloud/fog enabled && matching pass nullptr → false.
  bool record_post_opaque(render::rhi::Device* device, render::rhi::CommandList* list,
                          uint32_t width, uint32_t height,
                          const render::rhi::CameraMatrices* camera, int cloud_quality);

 private:
  OceanPass* ocean_pass_ = nullptr;
  CloudPass* cloud_pass_ = nullptr;
  SkyPass* sky_pass_ = nullptr;
  FogPass* fog_pass_ = nullptr;
  bool ocean_enabled_ = false;
  bool cloud_enabled_ = false;
  bool sky_enabled_ = false;
  bool fog_enabled_ = false;
};

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_ATMOSPHERE_FRAME_H_
