// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_ATMOSPHERE_ATMOSPHERE_SESSION_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_ATMOSPHERE_ATMOSPHERE_SESSION_H_

#include <memory>
#include <string>
#include <string_view>

#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "content/public/map_types.h"
#include "effect/atmosphere/cloud/cloud_pass.h"
#include "effect/atmosphere/fog/fog_pass.h"
#include "effect/atmosphere/frame/atmosphere_frame.h"
#include "effect/atmosphere/ocean/ocean_pass.h"
#include "effect/atmosphere/sky/sky_pass.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"

namespace content {

class MapScene;
class Scene3dGpuPresent;

// Atmosphere Environment + pass POD prep (ocean/cloud/sky/fog). Geo frame and
// world extent come from the bound Scene3dGpuPresent during prepare.
class AtmosphereSession {
 public:
  AtmosphereSession();
  ~AtmosphereSession();

  AtmosphereSession(const AtmosphereSession&) = delete;
  AtmosphereSession& operator=(const AtmosphereSession&) = delete;

  void bind_scene(const MapScene* scene);
  void bind_gpu(Scene3dGpuPresent* gpu);
  const MapScene* scene() const { return scene_; }

  gis::atmosphere::Environment* environment() { return atmosphere_.get(); }
  const gis::atmosphere::Environment* environment() const {
    return atmosphere_.get();
  }
  gis::atmosphere::Environment& ensure();

  void set_ocean_enabled(bool on);
  void set_cloud_enabled(bool on);
  void set_sky_enabled(bool on);
  void set_fog_enabled(bool on);
  void set_wind_overlay_enabled(bool on);
  bool wind_overlay_enabled() const { return wind_overlay_enabled_; }

  void set_time_sec(double t);
  double time_sec() const;

  bool load_fields(std::string_view spec);
  void seed_procedural();
  void enable_demo();

  // Project GIS samples onto pass POD (no CommandList). Uses bound gpu geo.
  bool prepare_for_present();
  void release_passes();

  // Ensure gpu geo_frame is valid for the current world extent (once per frame).
  bool ensure_geo_frame();

  effect::atmosphere::AtmosphereFrame& frame() { return atmosphere_frame_; }
  const effect::atmosphere::AtmosphereFrame& frame() const {
    return atmosphere_frame_;
  }

  // M3 city path self-test: DEM + 3D Tiles + atmosphere on/off.
  bool run_m3_self_test_hooks(std::string* err);

 private:
  gis::atmosphere::FieldGrid field_grid() const;
  Extent2 world_extent() const;
  bool prepare_ocean();
  bool prepare_clouds();
  bool prepare_sky();
  bool prepare_fog();

  const MapScene* scene_ = nullptr;
  Scene3dGpuPresent* gpu_ = nullptr;

  std::unique_ptr<gis::atmosphere::Environment> atmosphere_;
  effect::atmosphere::OceanPass ocean_pass_;
  effect::atmosphere::CloudPass cloud_pass_;
  effect::atmosphere::SkyPass sky_pass_;
  effect::atmosphere::FogPass fog_pass_;
  effect::atmosphere::AtmosphereFrame atmosphere_frame_;
  bool wind_overlay_enabled_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_ATMOSPHERE_ATMOSPHERE_SESSION_H_
