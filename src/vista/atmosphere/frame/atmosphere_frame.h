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

#include "vista/vista_export.h"

namespace vista {

class CloudPass;
class FogPass;
class GlobePass;
class OceanPass;
class SatCloudPass;
class SkyPass;

// Owns pass-order policy for environment draws around opaque geometry.
// Flat path: sky/depth → host opaque DEM → ocean → cloud/fog.
// Globe path: sky → globe DEM → sat clouds → fog. Flat ocean stays off.
class VISTA_EXPORT AtmosphereFrame {
 public:
  AtmosphereFrame() = default;
  ~AtmosphereFrame() = default;

  AtmosphereFrame(const AtmosphereFrame&) = delete;
  AtmosphereFrame& operator=(const AtmosphereFrame&) = delete;

  void set_ocean_pass(OceanPass* pass) { ocean_pass_ = pass; }
  void set_cloud_pass(CloudPass* pass) { cloud_pass_ = pass; }
  void set_sky_pass(SkyPass* pass) { sky_pass_ = pass; }
  void set_fog_pass(FogPass* pass) { fog_pass_ = pass; }
  void set_globe_pass(GlobePass* pass) { globe_pass_ = pass; }
  void set_sat_cloud_pass(SatCloudPass* pass) { sat_cloud_pass_ = pass; }

  // Mirrors session toggles projected from vista::AtmosphereParams.
  void set_ocean_enabled(bool on) { ocean_enabled_ = on; }
  void set_cloud_enabled(bool on) { cloud_enabled_ = on; }
  void set_sky_enabled(bool on) { sky_enabled_ = on; }
  void set_fog_enabled(bool on) { fog_enabled_ = on; }
  void set_globe_enabled(bool on) { globe_enabled_ = on; }
  void set_sat_cloud_enabled(bool on) { sat_cloud_enabled_ = on; }

  // Optional pre-opaque clear when sky is off (e.g. legacy stereo black).
  // When unset, sky-off clears to black (leftover parity) rather than navy.
  void set_clear_rgb(float r, float g, float b);
  void clear_clear_rgb();

  bool ocean_enabled() const { return ocean_enabled_; }
  bool cloud_enabled() const { return cloud_enabled_; }
  bool sky_enabled() const { return sky_enabled_; }
  bool fog_enabled() const { return fog_enabled_; }
  bool globe_enabled() const { return globe_enabled_; }
  bool sat_cloud_enabled() const { return sat_cloud_enabled_; }

  // True if pre-opaque pass will clear color (ocean and/or sky).
  bool clears_color() const;
  // True when opaque WorldPass should share depth with atmosphere passes.
  bool uses_shared_depth() const;

  // Sky and a depth clear, plus globe DEM when the globe path is on.
  // Flat ocean is not drawn here (it would cover DEM). Does not close the list.
  // Sky or globe enabled && matching pass nullptr → false (fail closed).
  bool record_pre_opaque(render::rhi::Device* device, render::rhi::CommandList* list,
                         uint32_t width, uint32_t height,
                         const render::rhi::CameraMatrices* camera);

  // Flat ocean, then sat clouds or volumetric clouds, then fog.
  // Ocean is before cloud/fog/sat. Ocean/cloud/fog/sat enabled && matching
  // pass nullptr → false (fail closed).
  bool record_post_opaque(render::rhi::Device* device, render::rhi::CommandList* list,
                          uint32_t width, uint32_t height,
                          const render::rhi::CameraMatrices* camera, int cloud_quality);

 private:
  OceanPass* ocean_pass_ = nullptr;
  CloudPass* cloud_pass_ = nullptr;
  SkyPass* sky_pass_ = nullptr;
  FogPass* fog_pass_ = nullptr;
  GlobePass* globe_pass_ = nullptr;
  SatCloudPass* sat_cloud_pass_ = nullptr;
  bool ocean_enabled_ = false;
  bool cloud_enabled_ = false;
  bool sky_enabled_ = false;
  bool fog_enabled_ = false;
  bool globe_enabled_ = false;
  bool sat_cloud_enabled_ = false;
  bool clear_rgb_set_ = false;
  float clear_r_ = 0.f;
  float clear_g_ = 0.f;
  float clear_b_ = 0.f;
};

}  // namespace vista

#endif  // EFFECT_ATMOSPHERE_ATMOSPHERE_FRAME_H_
