// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SCENE3D_CONTROLLER_H_
#define APP_VIEWS_SCENE3D_CONTROLLER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

#include "app/views/map_host_extent.h"
#include "content/public/map_types.h"
#include "gis/atmosphere/environment.h"
#include "gis/world/dem_frame.h"
#include "gis/world/scene.h"
#include "render/atmosphere/cloud_pass.h"
#include "render/atmosphere/ocean_pass.h"
#include "render/rhi/rhi.h"
#include "render/scene/scene.h"
#include "tool/gestures.h"

namespace content {
class MapContents;
}

namespace app {

class MapScene;

// Alias of gis::kDemDefaultOrbitYaw (south-of-target / 上北下南).
inline constexpr float kScene3dDefaultYaw = gis::kDemDefaultOrbitYaw;

// Chrome-side host of the shared leftover map scene (same MapContents session
// as SmartGis.exe): 2D ortho + 3D perspective, full China when contents or
// MapScene has it. Orbit / wheel-to-cursor / pinch update the camera; GPU
// present draws the land-masked DEM of that scene (not a cube). GDI fallback
// draws the mesh wireframe only when no shared present is hung.
class Scene3dController {
 public:
  Scene3dController();
  ~Scene3dController();

  Scene3dController(const Scene3dController&) = delete;
  Scene3dController& operator=(const Scene3dController&) = delete;

  void bind_map(const MapScene* scene);
  // Non-owning shared leftover session + OpenView id (kScene3d).
  void bind_contents(content::MapContents* session, uint32_t view_id);
  void reset();
  void apply_draft(const tool::Draft& draft);

  // Industry map gestures (Baidu / Google 2D, Cesium-style 3D). Client pixels.
  void apply_wheel_at(int view_x, int view_y, int32_t wheel, int view_w,
                      int view_h);
  void apply_pan(int dx_px, int dy_px);
  void apply_pinch(int view_x, int view_y, double scale, int view_w,
                   int view_h);

  // Shared leftover scene extent (lon/lat). China envelope when none yet.
  content::Extent2 world_extent() const;
  void apply_world_extent(const content::Extent2& e);
  void push_extent_to_contents();
  void pull_extent_from_contents();

  bool hosts_shared_scene() const {
    return contents_ != nullptr && view_id_ != 0;
  }

  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float distance() const { return distance_; }

  // Drop GPU mesh pointers without destroying buffers (Device owns them /
  // is shutting down). Call before MapViewport::detach / Device::shutdown.
  void abandon_mesh();

  // 3D perspective orbit of the shared scene.
  render::rhi::CameraMatrices camera_matrices(float aspect) const;
  // 2D ortho of the same lon/lat extent (leftover Map Edit / Map Data).
  render::rhi::CameraMatrices camera_matrices_ortho(float width_px,
                                                    float height_px) const;

  // Draw land mesh wireframe into |hdc| (view pixels). Used when no GPU /
  // no MapContents present. When |fill_background| is false, skip the solid
  // clear so chrome can overlay DEM strokes on an existing shared DIB.
  void paint(HDC hdc, int width_px, int height_px,
             bool fill_background = true) const;
  // Status text only (transparent) over a shared GPU / FlyCube present.
  void paint_hud(HDC hdc, int width_px, int height_px) const;

  // Record terrain + orbit camera on |device|, execute, and present. Returns
  // true when a GPU frame was submitted. When atmosphere is enabled, records
  // ocean → land (GpuScene) → clouds on the same CommandList.
  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px);

  // Optional atmosphere session. Default: no Environment (ocean/cloud off).
  // ensure_atmosphere() creates a disabled Environment; enable_atmosphere_demo()
  // seeds procedural WindNoise/CloudNoise/SeaMask and turns both passes on.
  // seed_atmosphere_procedural() seeds fields without flipping enable flags
  // (use with set_ocean_enabled / set_cloud_enabled for ocean-only demos).
  gis::atmosphere::Environment* atmosphere() { return atmosphere_.get(); }
  const gis::atmosphere::Environment* atmosphere() const {
    return atmosphere_.get();
  }
  gis::atmosphere::Environment& ensure_atmosphere();
  void set_ocean_enabled(bool on);
  void set_cloud_enabled(bool on);
  void set_wind_overlay_enabled(bool on);
  bool wind_overlay_enabled() const { return wind_overlay_enabled_; }

  // Scrub session clock via Environment::scrub_time_sec; clamps into any
  // timed External/Procedural range when one is present.
  void set_time_sec(double t);
  double time_sec() const;

  // Load External GeoTIFF series via Environment::load_external_series.
  // Spec (CLI-compatible): path[:channel[:time_sec]][,path...]
  // channel: wind_u|wind_v|wave_hs|wave_dir|cloud_cover|cloud_base|cloud_top|
  //          sea_mask (default cloud_cover). Entries with the same channel
  // are batched as one time series.
  bool load_atmosphere_fields(std::string_view spec);

  void seed_atmosphere_procedural();
  void enable_atmosphere_demo();

 private:
  void remember_view_size(int width_px, int height_px) const;
  void project(float x, float y, float z, int width_px, int height_px, int* sx,
               int* sy) const;
  void project_lon_lat(double lon, double lat, int width_px, int height_px,
                       int* sx, int* sy) const;
  void paint_wind_arrows(HDC hdc, int width_px, int height_px) const;
  void release_mesh();
  void rebuild_local_mesh();
  void release_atmosphere_passes();
  bool record_atmosphere_ocean(render::rhi::Device* device,
                               render::rhi::CommandList* list, uint32_t width,
                               uint32_t height,
                               const render::rhi::CameraMatrices& camera);
  bool record_atmosphere_clouds(render::rhi::Device* device,
                                render::rhi::CommandList* list, uint32_t width,
                                uint32_t height,
                                const render::rhi::CameraMatrices& camera);
  gis::atmosphere::FieldGrid atmosphere_field_grid() const;

  const MapScene* scene_ = nullptr;
  content::MapContents* contents_ = nullptr;
  uint32_t view_id_ = 0;
  content::Extent2 extent_{};
  // SP4: DEM mesh is seeded via gis::DemRaster → World; GpuScene mirrors for
  // present_gpu. No leftover dem_height_field_static link.
  gis::World terrain_world_;
  render::scene::GpuScene gpu_scene_;
  std::vector<float> local_xyz_;
  std::vector<unsigned> local_idx_;

  float yaw_ = kScene3dDefaultYaw;
  float pitch_ = 0.4f;
  float distance_ = 3.2f;
  int last_x_ = 0;
  int last_y_ = 0;
  mutable int last_w_ = 0;
  mutable int last_h_ = 0;
  bool has_last_ = false;

  // GDI wireframe fallback only (GPU path uses GpuScene meshes).
  render::rhi::Device* mesh_device_ = nullptr;

  std::unique_ptr<gis::atmosphere::Environment> atmosphere_;
  render::atmosphere::OceanPass ocean_pass_;
  render::atmosphere::CloudPass cloud_pass_;
  bool wind_overlay_enabled_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_SCENE3D_CONTROLLER_H_
