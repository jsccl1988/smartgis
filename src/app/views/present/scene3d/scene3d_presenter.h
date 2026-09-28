// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PRESENT_SCENE3D_PRESENTER_H_
#define APP_VIEWS_PRESENT_SCENE3D_PRESENTER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

#include "app/views/camera/map_host_extent.h"
#include "app/views/camera/orbit_frame.h"
#include "app/views/present/host/shell_overlay_effect.h"
#include "app/views/present/scene3d/frame/orbit_geo_frame.h"
#include "content/public/map_types.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"
#include "gis/vista/world/terrain/dem_frame.h"
#include "gis/vista/world/world.h"
#include "effect/atmosphere/frame/atmosphere_frame.h"
#include "effect/atmosphere/cloud/cloud_pass.h"
#include "effect/atmosphere/fog/fog_pass.h"
#include "effect/atmosphere/ocean/ocean_pass.h"
#include "effect/atmosphere/sky/sky_pass.h"
#include "effect/scene/scene.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "ui/gfx/raster/shell_raster.h"

namespace content {
class MapContents;
}

namespace app {

class MapScene;
class ViewFrame;

// Thin 3D present facade: bind orbit/map/contents, own mesh + atmosphere
// passes, and dispatch GPU present / GDI HUD. Device HWND binding lives in
// scene3d/session/; atmosphere field prep in scene3d/frame/; GDI in paint/.
class Scene3dPresenter {
 public:
  Scene3dPresenter();
  ~Scene3dPresenter();

  Scene3dPresenter(const Scene3dPresenter&) = delete;
  Scene3dPresenter& operator=(const Scene3dPresenter&) = delete;

  void bind_orbit(const OrbitFrame* orbit);
  void bind_map(const MapScene* scene);
  // 2D frame whose scale culls place-name labels drawn over the DEM.
  void bind_label_frame(const ViewFrame* frame);
  // Non-owning shared leftover session + OpenView id (kScene3d).
  void bind_contents(content::MapContents* session, uint32_t view_id);
  void reset();
  void apply_draft(const tool::Draft& draft);


  // Shared leftover scene extent (lon/lat). China envelope when none yet.
  content::Extent2 world_extent() const;

  bool hosts_shared_scene() const {
    return contents_ != nullptr && view_id_ != 0;
  }


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
  // clear so shell can overlay DEM strokes on an existing shared DIB.
  void paint(HDC hdc, int width_px, int height_px,
             bool fill_background = true) const;
  // Status text only (transparent) over a shared GPU / FlyCube present.
  void paint_hud(HDC hdc, int width_px, int height_px) const;

  // Record terrain + orbit camera on |device| and present. Returns true when
  // a GPU frame was submitted. Effects are atmosphere pre, opaque terrain,
  // atmosphere post, then optional DrawRequest.shell HUD, on one command list.
  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px,
                   const ui::gfx::ShellRaster* shell = nullptr,
                   uint64_t shell_generation = 0);

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
  void set_sky_enabled(bool on);
  void set_fog_enabled(bool on);
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

  // M3 city path self-test: DEM seed + 3D Tiles select/stream with content
  // cache cap + atmosphere on/off. On failure writes a short tag into |err|
  // (e.g. "m3-dem-ok") when non-null. Parent hosts may map tags to exit 90�?2.
  bool run_m3_self_test_hooks(std::string* err);

  // TIN / DEM triangle-edge overlay. Env SMT_SCENE3D_WIREFRAME=1 turns on at
  // construction; hosts may toggle at runtime. Default filled only.
  void set_wireframe_enabled(bool on);
  bool wireframe_enabled() const { return wireframe_enabled_; }

  // Last successful present backend label (e.g. "FlyCube/DX12"), or fallback
  // string when painting GDI / stereo. |name| must outlive the controller
  // (string literals / backend_display_name only).
  const char* render_engine_name() const { return render_engine_name_; }
  void set_render_engine_name(const char* name) const;

  // Bottom-right engine badge (white on dark). Used by paint_hud and by hosts
  // that present ContentMapView without a full Scene3dPresenter paint.
  static void paint_engine_logo(HDC hdc, int width_px, int height_px,
                                const char* engine_name);

 private:
  void remember_view_size(int width_px, int height_px) const;
  void project(float x, float y, float z, int width_px, int height_px, int* sx,
               int* sy) const;
  void project_lon_lat(double lon, double lat, int width_px, int height_px,
                       int* sx, int* sy) const;
  void paint_wind_arrows(HDC hdc, int width_px, int height_px) const;
  void paint_wireframe_edges(HDC hdc, int width_px, int height_px) const;
  void release_mesh();
  void rebuild_local_mesh();
  void release_atmosphere_passes();
  // DXGI/GL flip surfaces ignore GDI on the present HWND; a WS_CHILD badge
  // composites via DWM over the swapchain. Mem-DC BitBlt paths bake into HDC.
  void sync_engine_logo_overlay(HWND parent, int width_px,
                                int height_px) const;
  void hide_engine_logo_overlay() const;
  void release_engine_logo_overlay() const;
  // Project GIS ocean/cloud/sky/fog samples onto pass POD (no CommandList).
  bool prepare_atmosphere_ocean();
  bool prepare_atmosphere_clouds();
  bool prepare_atmosphere_sky();
  bool prepare_atmosphere_fog();
  gis::atmosphere::FieldGrid atmosphere_field_grid() const;

  const MapScene* scene_ = nullptr;
  const OrbitFrame* orbit_ = nullptr;
  const ViewFrame* label_frame_ = nullptr;
  content::MapContents* contents_ = nullptr;
  uint32_t view_id_ = 0;
  // SP4: DEM mesh is seeded via gis::DemRaster �?World; GpuScene mirrors for
  // present_gpu. No leftover dem_height_field_static link.
  gis::World terrain_world_;
  effect::scene::GpuScene gpu_scene_;
  std::vector<float> local_xyz_;
  std::vector<unsigned> local_idx_;


  // GDI wireframe fallback only (GPU path uses GpuScene meshes).
  render::rhi::Device* mesh_device_ = nullptr;
  mutable const char* render_engine_name_ = "pending";
  mutable HWND logo_hwnd_ = nullptr;
  bool wireframe_enabled_ = false;

  std::unique_ptr<gis::atmosphere::Environment> atmosphere_;
  effect::atmosphere::OceanPass ocean_pass_;
  effect::atmosphere::CloudPass cloud_pass_;
  effect::atmosphere::SkyPass sky_pass_;
  effect::atmosphere::FogPass fog_pass_;
  effect::atmosphere::AtmosphereFrame atmosphere_frame_;
  detail::ShellOverlayEffect shell_overlay_;
  OrbitGeoFrame geo_frame_;
  bool wind_overlay_enabled_ = false;
  // Last DEM max_edge used by rebuild_local_mesh (distance LOD).
  int terrain_lod_edge_ = 0;
  float yaw() const {
    return orbit_ ? orbit_->yaw() : kScene3dDefaultYaw;
  }
  float pitch() const { return orbit_ ? orbit_->pitch() : 0.4f; }
  float distance() const {
    return orbit_ ? orbit_->distance() : 3.2f;
  }

};

}  // namespace app

#endif  // APP_VIEWS_PRESENT_SCENE3D_PRESENTER_H_
