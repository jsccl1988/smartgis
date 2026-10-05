// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_ATMOSPHERE_ATMOSPHERE_SESSION_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_ATMOSPHERE_ATMOSPHERE_SESSION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "content/public/map_types.h"
#include "vista/pass/atmosphere/cloud/cloud_pass.h"
#include "vista/pass/atmosphere/fog/fog_pass.h"
#include "vista/pass/atmosphere/atmosphere_frame.h"
#include "vista/pass/atmosphere/globe/globe_pass.h"
#include "vista/pass/atmosphere/globe/sat_cloud_pass.h"
#include "vista/pass/atmosphere/ocean/ocean_pass.h"
#include "vista/pass/atmosphere/sky/sky_pass.h"
#include "vista/component/atmosphere/environment.h"

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

  vista::atmosphere::Environment* environment() { return atmosphere_.get(); }
  const vista::atmosphere::Environment* environment() const {
    return atmosphere_.get();
  }
  vista::atmosphere::Environment& ensure();

  void set_ocean_enabled(bool on);
  void set_cloud_enabled(bool on);
  void set_sky_enabled(bool on);
  void set_fog_enabled(bool on);
  void set_wind_overlay_enabled(bool on);
  bool wind_overlay_enabled() const { return wind_overlay_enabled_; }

  // Google-Earth-like stack: DEM globe + satellite cloud shell (+ sky/fog).
  void set_globe_enabled(bool on);
  void set_sat_cloud_enabled(bool on);
  bool globe_enabled() const { return globe_enabled_; }
  bool sat_cloud_enabled() const { return sat_cloud_enabled_; }

  // Origin-style jet surface + isolines on the China DEM globe window.
  void set_elevation_overlay(bool surface, bool curves);
  bool elevation_surface_overlay() const;
  bool elevation_curve_overlay() const;

  // Enable ContourSheet (curves + TIN + side color scale), rebuild from
  // WaveHs, and push a stacked overlay TIN (planar) / elevation overlay
  // (globe). Default face for world3d.open_earth / China Scene3D seed.
  bool apply_contour_suite_defaults();

  // Orbit distance in globe radii (1 = surface). Loads china_dem as a
  // blended overlay below ~3.2 R and reaches full mix by ~1.65 R. When
  // |look_lon_deg|/|look_lat_deg| sit over open water, enables OceanPass.
  void update_globe_detail_lod(float orbit_distance);
  void update_globe_detail_lod(float orbit_distance, double look_lon_deg,
                               double look_lat_deg);

  void set_time_sec(double t);
  double time_sec() const;

  bool load_fields(std::string_view spec);
  // Seed FieldStore (with land rings from MapScene when available).
  void seed_procedural();
  // Same seed; when with_land_rings is false, skip polygon export (showcase
  // capture must not stall on dense area layers).
  void seed_procedural(bool with_land_rings);
  void enable_demo();

  // Wall-clock advance for ocean FFT / cloud animation. Called from
  // prepare_for_present; also usable by showcase linger loops.
  void advance_sim_time();

  // Project GIS samples onto pass POD (no CommandList). Uses bound gpu geo.
  bool prepare_for_present();
  void release_passes();

  // True when ocean/cloud need continuous MapViewport BeginFrame pacing.
  bool needs_continuous_present() const;

  // Ensure gpu geo_frame is valid for the current world extent (once per frame).
  bool ensure_geo_frame();

  vista::AtmosphereFrame& frame() { return atmosphere_frame_; }
  const vista::AtmosphereFrame& frame() const {
    return atmosphere_frame_;
  }

  // Ocean draw after opaque DEM (Scene3dGpuPresent two-phase present). Height
  // SRV must not stay bound on slot 0 when the DEM textured pass runs.
  vista::OceanPass& ocean_pass() { return ocean_pass_; }
  const vista::OceanPass& ocean_pass() const { return ocean_pass_; }

  vista::GlobePass& globe_pass() { return globe_pass_; }
  const vista::GlobePass& globe_pass() const { return globe_pass_; }
  vista::SatCloudPass& sat_cloud_pass() { return sat_cloud_pass_; }
  const vista::SatCloudPass& sat_cloud_pass() const {
    return sat_cloud_pass_;
  }

  // M3 city path self-test: DEM + 3D Tiles + atmosphere on/off.
  bool run_m3_self_test_hooks(std::string* err);

 private:
  vista::atmosphere::FieldGrid field_grid() const;
  Extent2 world_extent() const;
  bool prepare_ocean();
  bool prepare_clouds();
  bool prepare_sky();
  bool prepare_fog();
  bool prepare_globe();
  bool prepare_sat_clouds();
  bool load_china_globe_detail();
  void apply_globe_sea_ocean(double lon_deg, double lat_deg, bool on);

  const MapScene* scene_ = nullptr;
  Scene3dGpuPresent* gpu_ = nullptr;

  // Flags first — pass POD sizes shift often; keep enable bits at stable
  // offsets so inlined getters in other TUs cannot read a stale layout.
  bool wind_overlay_enabled_ = false;
  bool globe_enabled_ = false;
  bool sat_cloud_enabled_ = false;
  bool elevation_surface_overlay_ = false;
  bool elevation_curve_overlay_ = false;
  bool globe_surface_loaded_ = false;
  bool china_globe_detail_loaded_ = false;
  bool sat_cloud_cover_loaded_ = false;
  // QPC tick of the last advance_sim_time(); 0 = not primed.
  std::uint64_t last_sim_qpc_ = 0;

  vista::atmosphere::EnvironmentPtr atmosphere_;
  vista::OceanPass ocean_pass_;
  vista::CloudPass cloud_pass_;
  vista::SkyPass sky_pass_;
  vista::FogPass fog_pass_;
  vista::GlobePass globe_pass_;
  vista::SatCloudPass sat_cloud_pass_;
  vista::AtmosphereFrame atmosphere_frame_;

  // Sea-mask FieldStore sample is static for a fixed geo extent; refill only
  // when the orbit frame extent changes (was 32x32 samples every present).
  Extent2 cached_sea_mask_extent_{};
  std::vector<float> cached_sea_mask_;
  int cached_sea_mask_n_ = 0;
  bool sea_mask_cache_valid_ = false;

  // Idempotent seed_procedural across Map↔Scene3D tab switches.
  bool procedural_seeded_ = false;
  bool procedural_seed_with_rings_ = false;
  const MapScene* procedural_seed_scene_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_ATMOSPHERE_ATMOSPHERE_SESSION_H_
