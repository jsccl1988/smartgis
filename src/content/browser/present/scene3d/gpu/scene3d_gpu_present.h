// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_GPU_SCENE3D_GPU_PRESENT_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_GPU_SCENE3D_GPU_PRESENT_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "base/time/frame_timer.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/host/shell_overlay_effect.h"
#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "content/browser/present/scene3d/frame/tileset_stream.h"
#include "content/public/map_types.h"
#include "vista/world_gpu/pass.h"
#include "vista/world/world.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"

namespace content {

class AtmosphereSession;
class MapScene;

// Product look for Scene3D: default atmosphere vs leftover stereo parity.
enum class Scene3dLookPreset {
  kAtmosphere = 0,
  kLegacyStereo = 1,
};

// Screen-space / orbit place-name for leftover-style stereo labels.
struct Scene3dLegacyLabel {
  std::string text;
  double lon = 0;
  double lat = 0;
};

// GPU present path for 3D: DEM mesh / WorldPass / shell overlay / present mutex.
class Scene3dGpuPresent {
 public:
  Scene3dGpuPresent() = default;
  ~Scene3dGpuPresent() = default;

  Scene3dGpuPresent(const Scene3dGpuPresent&) = delete;
  Scene3dGpuPresent& operator=(const Scene3dGpuPresent&) = delete;

  void bind_orbit(const OrbitFrame* orbit);
  void bind_map(const MapScene* scene);

  // Drop GPU mesh pointers without destroying Device-owned buffers.
  // Overlay geographic cloud is kept so the next present can re-attach.
  void abandon(AtmosphereSession* atmosphere);

  bool present(render::rhi::Device* device, uint32_t width_px,
               uint32_t height_px, const ui::gfx::ShellRaster* shell,
               uint64_t shell_generation, AtmosphereSession& atmosphere);

  void set_wireframe_enabled(bool on);
  bool wireframe_enabled() const { return wireframe_enabled_; }

  // Atmosphere (default) vs leftover stereo look (black clear + hypsometric).
  void set_look_preset(Scene3dLookPreset preset);
  Scene3dLookPreset look_preset() const { return look_preset_; }

  // Best-effort China coastlines + place-name labels for kLegacyStereo.
  // Returns true when at least one vector node or label was attached.
  bool ensure_legacy_overlays();
  int legacy_label_count() const { return static_cast<int>(legacy_labels_.size()); }
  bool has_legacy_coast_vectors() const { return legacy_coast_seeded_; }
  const std::vector<Scene3dLegacyLabel>& legacy_labels() const {
    return legacy_labels_;
  }

  // Geographic overlay points (lon, lat, elev interleaved). Re-attached in
  // orbit space after each terrain rebuild so GPU present shows RGB cloud.
  void set_overlay_pointcloud(const float* xyz_lon_lat_elev, int point_count,
                              const uint8_t* rgba);
  void clear_overlay_pointcloud();

  // Geographic TIN (lon/lat/elev + triangle indices). Attached as a separate
  // kTerrain node after each DEM rebuild (stratum / storm-surge free surface).
  // Optional |albedo_rgba| (4 bytes) drapes a solid tint so water stays cyan
  // instead of the untextured land-green default in WorldPass.
  void set_overlay_tin_mesh(const float* xyz_lon_lat_elev, int point_count,
                            const unsigned* indices, int index_count,
                            const uint8_t* albedo_rgba = nullptr);
  void clear_overlay_tin_mesh();

  // Attach an in-memory 3D Tiles JSON (city fixture) into the present World.
  // Each present() pumps select → ensure_tileset_content under the LRU budget.
  bool attach_tileset_json(const char* json, size_t len, const char* name);
  void set_tileset_content_root(const std::string& root);
  void clear_tileset();
  TilesetStreamSession* tileset_stream();
  const TilesetStreamSession* tileset_stream() const;

  // Caller must hold present_mu_. Returns nullptr for unset / MSVC freefill.
  TilesetStreamSession* live_tileset_stream_locked();
  const TilesetStreamSession* live_tileset_stream_locked() const;

  const OrbitFrame* orbit() const { return orbit_; }
  const MapScene* scene() const { return scene_; }

  Extent2 world_extent() const;
  OrbitGeoFrame& geo_frame() { return geo_frame_; }
  const OrbitGeoFrame& geo_frame() const { return geo_frame_; }

  std::mutex& mutex() const { return present_mu_; }

  const std::vector<float>& local_xyz() const { return local_xyz_; }
  const std::vector<unsigned>& local_idx() const { return local_idx_; }
  // Geographic overlay beads for GDI stick/marker paint (lon/lat/elev).
  const std::vector<float>& overlay_xyz_geo() const { return overlay_xyz_geo_; }
  const std::vector<uint8_t>& overlay_rgba() const { return overlay_rgba_; }
  // Index into local_idx_ where DEM tris end and overlay TIN tris begin
  // (software paint uses this to tint free-surface water cyan).
  size_t dem_local_idx_count() const { return dem_local_idx_count_; }
  bool overlay_tin_has_albedo() const { return overlay_tin_has_albedo_; }
  const uint8_t* overlay_tin_albedo() const { return overlay_tin_albedo_; }

  // Caller must hold mutex(). Used by software paint path.
  // Returns true when DEM LOD/extent changed (overlays must re-attach).
  bool rebuild_local_mesh();

  // Attach overlay_pointcloud_* into terrain_world_ (caller holds mutex).
  // When |force| is false, skip if the overlay node is already present.
  void attach_overlay_pointcloud_locked(bool force = true);
  // Attach overlay TIN World node + fold into local_* for GDI (caller holds mutex).
  void attach_overlay_tin_locked(bool force = true);

  float yaw() const;
  float pitch() const;
  float distance() const;

  render::rhi::CameraMatrices camera_matrices(float aspect) const;
  render::rhi::CameraMatrices camera_matrices_ortho(float width_px,
                                                    float height_px) const;

  void remember_view_size(int width_px, int height_px) const;

  // Last present/paint sets this for HUD badge (shared with software painter).
  mutable const char* render_engine_name = "pending";
  // Present cadence for HUD "Fps%.3f" (legacy SmtScene::Render).
  mutable float last_fps = 0.f;
  // Stable "EngineName  FpsN.NNN" for the bottom-right logo HWND.
  mutable char engine_fps_label_[96] = {};

  void note_present_frame() const {
    hud_fps_timer_.update();
    last_fps = hud_fps_timer_.get_fps();
    const char* name =
        (render_engine_name && render_engine_name[0]) ? render_engine_name
                                                     : "unknown";
    std::snprintf(engine_fps_label_, sizeof(engine_fps_label_),
                  "%s  Fps%.3f", name, last_fps);
  }

 private:
  const MapScene* scene_ = nullptr;
  const OrbitFrame* orbit_ = nullptr;

  // STL / sync first — WorldPass and World are large blobs; keep mutex and
  // vectors ahead so a size-skew overwrite into those blobs cannot clobber
  // lock state or container proxies (Debug AV on clear / unlock).
  std::vector<float> local_xyz_;
  std::vector<unsigned> local_idx_;
  // DEM-only sizes before overlay fold (cache-hit presents must not stack).
  size_t dem_local_xyz_count_ = 0;
  size_t dem_local_idx_count_ = 0;
  int terrain_lod_edge_ = 0;
  mutable std::mutex present_mu_;
  mutable base::FrameTimer hud_fps_timer_;
  render::rhi::Device* mesh_device_ = nullptr;
  bool wireframe_enabled_ = false;

  vista::World terrain_world_;
  vista::WorldPass gpu_scene_;
  detail::ShellOverlayEffect shell_overlay_;
  OrbitGeoFrame geo_frame_;
  std::unique_ptr<TilesetStreamSession> tileset_stream_;

  // Geographic lon/lat/elev + optional RGBA8; applied after DEM rebuild.
  std::vector<float> overlay_xyz_geo_;
  std::vector<uint8_t> overlay_rgba_;
  // Geographic TIN verts (lon/lat/elev) + triangle indices.
  std::vector<float> overlay_tin_xyz_geo_;
  std::vector<unsigned> overlay_tin_idx_;
  uint8_t overlay_tin_albedo_[4] = {46, 170, 220, 230};
  bool overlay_tin_has_albedo_ = false;

  Scene3dLookPreset look_preset_ = Scene3dLookPreset::kAtmosphere;
  std::vector<Scene3dLegacyLabel> legacy_labels_;
  bool legacy_coast_seeded_ = false;
  bool legacy_overlays_attempted_ = false;

  // After ocean height / sky depth first alloc, DEM albedo must re-upload once
  // so FlyCube heap recycling cannot leave stale SRVs. Warm frames skip.
  bool dem_gpu_synced_after_ocean_ = false;
  bool dem_gpu_synced_after_sky_ = false;
  bool overlay_pointcloud_dirty_ = false;
  bool overlay_tin_dirty_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_GPU_SCENE3D_GPU_PRESENT_H_
