// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/gpu/present/scene3d_gpu_present.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <vector>

#include "base/core/log.h"
#include "base/time/elapsed_timer.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/frame/terrain/terrain_mesh.h"
#include "content/browser/present/scene3d/frame/tileset/tileset_stream.h"
#include "content/browser/present/scene3d/profile/scene3d_phase_profile.h"
#include "content/browser/camera/orbit_frame.h"
#include "vista/pass/world/atmosphere/atmosphere_effects.h"
#include "vista/pass/world/hud/hud_build.h"
#include "vista/pass/world/hud/hud_effect.h"
#include "vista/pass/world/opaque_effect.h"
#include "vista/pass/world/pass.h"
#include "vista/component/world/atmosphere/environment.h"
#include "vista/terrain/dem/dem_bake_cache.h"
#include "vista/terrain/dem/dem_frame.h"
#include "vista/terrain/dem/raster/dem_raster.h"
#include "render/graph/frame_graph.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"

namespace content {
namespace {

// MSVC Debug freefill / freed markers. unique_ptr bool is true for 0xCD? so
// `if (tileset_stream_)` alone is not enough after a layout-skewed GpuPresent.
bool ptr_addr_poison(uintptr_t addr) {
  if (addr < 0x10000u) {
    return true;
  }
  const auto lo24 = addr & 0xffffff00ull;
  return lo24 == 0xcdcdcd00ull || lo24 == 0xdddddd00ull ||
         lo24 == 0xcccccc00ull || lo24 == 0xfeeefeeeull ||
         lo24 == 0xababab00ull;
}

struct HudProjectCtx {
  const OrbitFrame* orbit = nullptr;
  int width_px = 0;
  int height_px = 0;
};

bool hud_project_lon_lat(void* ctx, double lon, double lat, float* sx,
                         float* sy) {
  auto* c = static_cast<HudProjectCtx*>(ctx);
  if (!c || !c->orbit || !sx || !sy) {
    return false;
  }
  int ix = 0;
  int iy = 0;
  c->orbit->project_lon_lat(lon, lat, c->width_px, c->height_px, &ix, &iy);
  *sx = static_cast<float>(ix);
  *sy = static_cast<float>(iy);
  return true;
}

void fill_scene3d_hud_frame(vista::HudFrame* frame, Scene3dGpuPresent* gpu,
                            AtmosphereSession& atmosphere, uint32_t width_px,
                            uint32_t height_px, uint64_t generation) {
  if (!frame || !gpu || width_px == 0 || height_px == 0) {
    return;
  }
  frame->clear();
  frame->width_px = width_px;
  frame->height_px = height_px;
  frame->generation = generation;

  const vista::atmosphere::Environment* env = atmosphere.environment();
  vista::hud_append_status_lines(
      frame, gpu->yaw(), gpu->pitch(), gpu->distance(), gpu->last_fps,
      gpu->render_engine_name, gpu->wireframe_enabled(),
      gpu->hosts_shared_scene(), env != nullptr,
      static_cast<float>(atmosphere.time_sec()),
      env && env->ocean_enabled(), env && env->cloud_enabled(),
      atmosphere.wind_overlay_enabled());
  vista::hud_append_compass(frame, gpu->yaw(), vista::kDemDefaultOrbitYaw);
  vista::hud_append_engine_badge(
      frame, gpu->engine_fps_label_[0] ? gpu->engine_fps_label_
                                      : gpu->render_engine_name);

  if (gpu->look_preset() == Scene3dLookPreset::kLegacyStereo) {
    for (const Scene3dLegacyLabel& lab : gpu->legacy_labels()) {
      int sx = 0;
      int sy = 0;
      if (gpu->orbit()) {
        gpu->orbit()->project_lon_lat(lab.lon, lab.lat,
                                      static_cast<int>(width_px),
                                      static_cast<int>(height_px), &sx, &sy);
      }
      if (sx < -40 || sy < -20 || sx > static_cast<int>(width_px) + 40 ||
          sy > static_cast<int>(height_px) + 20) {
        continue;
      }
      vista::hud_append_place_label(frame, lab.text.c_str(),
                                    static_cast<float>(sx),
                                    static_cast<float>(sy));
    }
  }

  if (atmosphere.wind_overlay_enabled() && env) {
    const Extent2 e = gpu->world_extent();
    HudProjectCtx pctx{gpu->orbit(), static_cast<int>(width_px),
                       static_cast<int>(height_px)};
    vista::hud_append_wind_arrows(frame, &env->field_store(),
                                  atmosphere.time_sec(), e.xmin, e.ymin, e.xmax,
                                  e.ymax, hud_project_lon_lat, &pctx);
  }
}

}  // namespace

Scene3dGpuPresent::Scene3dGpuPresent() = default;

Scene3dGpuPresent::~Scene3dGpuPresent() {
  prep_mailbox_.invalidate();
  if (prep_staging_) {
    std::lock_guard<std::mutex> lock(prep_staging_->mu);
    prep_staging_->blob.reset();
  }
}

TilesetStreamSession* Scene3dGpuPresent::live_tileset_stream_locked() {
  TilesetStreamSession* stream = tileset_stream_.get();
  if (!stream) {
    return nullptr;
  }
  if (ptr_addr_poison(reinterpret_cast<uintptr_t>(stream))) {
    // Drop the freefill pointer without operator delete (not a real object).
    (void)tileset_stream_.release();
    return nullptr;
  }
  return stream;
}

const TilesetStreamSession* Scene3dGpuPresent::live_tileset_stream_locked()
    const {
  return const_cast<Scene3dGpuPresent*>(this)->live_tileset_stream_locked();
}

void Scene3dGpuPresent::bind_orbit(const OrbitFrame* orbit) {
  orbit_ = orbit;
}

void Scene3dGpuPresent::bind_scene(const GisScene* scene) {
  scene_ = scene;
  local_xyz_.clear();
  local_idx_.clear();
  terrain_lod_edge_ = 0;
  // Hide dem_bake mkdir / root resolve before the first cold present.
  vista::dem_bake_cache_warmup();
}

void Scene3dGpuPresent::set_wireframe_enabled(bool on) {
  wireframe_enabled_ = on;
  gpu_scene_.set_wireframe(on);
}

void Scene3dGpuPresent::set_look_preset(Scene3dLookPreset preset) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  // Drop leftover labels only when leaving kLegacyStereo. Unconditional
  // swap-drop on every atmosphere set raced with software paint iterating
  // legacy_labels_ without the mutex (select_view_tab 鈫?string dtor AV at
  // 0xC0000005). Paint already gates on look_preset_ == kLegacyStereo.
  const bool leaving_legacy =
      look_preset_ == Scene3dLookPreset::kLegacyStereo &&
      preset != Scene3dLookPreset::kLegacyStereo;
  look_preset_ = preset;
  if (leaving_legacy) {
    std::vector<Scene3dLegacyLabel> drop;
    legacy_labels_.swap(drop);
    legacy_coast_seeded_ = false;
    legacy_overlays_attempted_ = false;
  }
}

bool Scene3dGpuPresent::ensure_legacy_overlays() {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  return ensure_legacy_overlays_locked();
}

bool Scene3dGpuPresent::ensure_legacy_overlays_locked() {
  legacy_overlays_attempted_ = true;
  if (legacy_labels_.empty()) {
    // Major China place-names (leftover stereo label batch character).
    // UTF-8 as \x escapes into const char* (no u8 / char8_t); paint uses CP_UTF8.
    static const struct {
      const char* text;
      double lon;
      double lat;
    } kCities[] = {
        {"\xe4\xb9\x8c\xe9\xb2\x81\xe6\x9c\xa8\xe9\xbd\x90", 87.62, 43.83},
        {"\xe6\x8b\x89\xe8\x90\xa8", 91.11, 29.97},
        {"\xe8\xa5\xbf\xe5\xae\x81", 101.78, 36.62},
        {"\xe5\x85\xb0\xe5\xb7\x9e", 103.83, 36.06},
        {"\xe9\x93\xb6\xe5\xb7\x9d", 106.27, 38.47},
        {"\xe5\x91\xbc\xe5\x92\x8c\xe6\xb5\xa9\xe7\x89\xb9", 111.75, 40.84},
        {"\xe5\x93\x88\xe5\xb0\x94\xe6\xbb\xa8", 126.53, 45.80},
        {"\xe9\x95\xbf\xe6\x98\xa5", 125.32, 43.88},
        {"\xe6\xb2\x88\xe9\x98\xb3", 123.43, 41.80},
        {"\xe5\x8c\x97\xe4\xba\xac", 116.40, 39.90},
        {"\xe5\xa4\xa9\xe6\xb4\xa5", 117.20, 39.08},
        {"\xe7\x9f\xb3\xe5\xae\xb6\xe5\xba\x84", 114.51, 38.04},
        {"\xe5\xa4\xaa\xe5\x8e\x9f", 112.55, 37.87},
        {"\xe6\xb5\x8e\xe5\x8d\x97", 117.00, 36.65},
        {"\xe9\x83\x91\xe5\xb7\x9e", 113.62, 34.75},
        {"\xe8\xa5\xbf\xe5\xae\x89", 108.94, 34.34},
        {"\xe5\x8d\x97\xe4\xba\xac", 118.78, 32.06},
        {"\xe4\xb8\x8a\xe6\xb5\xb7", 121.47, 31.23},
        {"\xe6\x9d\xad\xe5\xb7\x9e", 120.15, 30.28},
        {"\xe5\x90\x88\xe8\x82\xa5", 117.28, 31.86},
        {"\xe7\xa6\x8f\xe5\xb7\x9e", 119.30, 26.08},
        {"\xe5\x8d\x97\xe6\x98\x8c", 115.86, 28.68},
        {"\xe6\xad\xa6\xe6\xb1\x89", 114.31, 30.57},
        {"\xe9\x95\xbf\xe6\xb2\x99", 112.98, 28.19},
        {"\xe5\xb9\xbf\xe5\xb7\x9e", 113.26, 23.13},
        {"\xe5\x8d\x97\xe5\xae\x81", 108.37, 22.82},
        {"\xe6\xb5\xb7\xe5\x8f\xa3", 110.35, 20.02},
        {"\xe6\x88\x90\xe9\x83\xbd", 104.06, 30.67},
        {"\xe9\x87\x8d\xe5\xba\x86", 106.55, 29.56},
        {"\xe8\xb4\xb5\xe9\x98\xb3", 106.63, 26.65},
        {"\xe6\x98\x86\xe6\x98\x8e", 102.71, 25.04},
        {"\xe5\x8f\xb0\xe5\x8c\x97", 121.56, 25.04},
    };
    legacy_labels_.reserve(sizeof(kCities) / sizeof(kCities[0]));
    for (const auto& c : kCities) {
      legacy_labels_.push_back(Scene3dLegacyLabel{c.text, c.lon, c.lat});
    }
  }

  // Coast / admin lines: document vectors (china_city) when already loaded.
  if (scene_ && scene_->feature_count() > 0) {
    legacy_coast_seeded_ = true;
  }
  return !legacy_labels_.empty();
}

void Scene3dGpuPresent::set_overlay_pointcloud(const float* xyz_lon_lat_elev,
                                               int point_count,
                                               const uint8_t* rgba) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  overlays_.set_pointcloud(xyz_lon_lat_elev, point_count, rgba);
}

void Scene3dGpuPresent::clear_overlay_pointcloud() {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  overlays_.clear_pointcloud();
}

void Scene3dGpuPresent::set_overlay_tin_mesh(const float* xyz_lon_lat_elev,
                                             int point_count,
                                             const unsigned* indices,
                                             int index_count,
                                             const uint8_t* albedo_rgba) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  overlays_.set_tin(xyz_lon_lat_elev, point_count, indices, index_count,
                    albedo_rgba);
}

void Scene3dGpuPresent::set_overlay_tin_drape(const uint8_t* rgba, uint32_t width,
                                              uint32_t height, const float* uv,
                                              int uv_float_count) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  overlays_.set_tin_drape(rgba, width, height, uv, uv_float_count);
}

void Scene3dGpuPresent::clear_overlay_tin_mesh() {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  overlays_.clear_tin();
}

void Scene3dGpuPresent::set_dem_drape_rgba(const uint8_t* rgba, uint32_t width,
                                           uint32_t height) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  dem_drape_rgba_.clear();
  dem_drape_w_ = 0;
  dem_drape_h_ = 0;
  if (!rgba || width == 0 || height == 0) {
    return;
  }
  const size_t need =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  dem_drape_rgba_.assign(rgba, rgba + need);
  dem_drape_w_ = width;
  dem_drape_h_ = height;
}

void Scene3dGpuPresent::clear_dem_drape() {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  dem_drape_rgba_.clear();
  dem_drape_w_ = 0;
  dem_drape_h_ = 0;
}

void Scene3dGpuPresent::apply_dem_drape_locked() {
  if (dem_drape_rgba_.empty() || dem_drape_w_ == 0 || dem_drape_h_ == 0) {
    return;
  }
  for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
    vista::Node* n = const_cast<vista::Node*>(terrain_world_.node_at(i));
    if (!n || n->kind != vista::NodeKind::kTerrain) {
      continue;
    }
    if (n->name == "overlay_tin") {
      continue;
    }
    n->terrain.rgba = dem_drape_rgba_;
    n->terrain.tex_w = dem_drape_w_;
    n->terrain.tex_h = dem_drape_h_;
  }
}

void Scene3dGpuPresent::set_studio_block(bool on) {
  studio_block_ = on;
}

bool Scene3dGpuPresent::attach_tileset_json(const char* json, size_t len,
                                            const char* name) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  TilesetStreamSession* stream = live_tileset_stream_locked();
  if (!stream) {
    tileset_stream_ = std::make_unique<TilesetStreamSession>();
    stream = tileset_stream_.get();
  }
  return stream->attach_json(&terrain_world_, json, len, name);
}

void Scene3dGpuPresent::set_tileset_content_root(const std::string& root) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  TilesetStreamSession* stream = live_tileset_stream_locked();
  if (!stream) {
    tileset_stream_ = std::make_unique<TilesetStreamSession>();
    stream = tileset_stream_.get();
  }
  stream->set_content_root(root);
}

void Scene3dGpuPresent::clear_tileset() {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  TilesetStreamSession* stream = live_tileset_stream_locked();
  if (!stream) {
    return;
  }
  stream->clear(&terrain_world_);
}

TilesetStreamSession* Scene3dGpuPresent::tileset_stream() {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  return live_tileset_stream_locked();
}

const TilesetStreamSession* Scene3dGpuPresent::tileset_stream() const {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  return live_tileset_stream_locked();
}

void Scene3dGpuPresent::attach_overlay_pointcloud_locked(bool force) {
  overlays_.attach_pointcloud(&terrain_world_, geo_frame_, local_xyz_,
                              dem_local_xyz_count_, force);
}

void Scene3dGpuPresent::attach_overlay_tin_locked(bool force) {
  overlays_.attach_tin(&terrain_world_, geo_frame_, &local_xyz_, &local_idx_,
                       dem_local_xyz_count_, dem_local_idx_count_, force);
}

float Scene3dGpuPresent::yaw() const {
  return orbit_ ? orbit_->yaw() : kScene3dDefaultYaw;
}

float Scene3dGpuPresent::pitch() const {
  return orbit_ ? orbit_->pitch() : 0.4f;
}

float Scene3dGpuPresent::distance() const {
  return orbit_ ? orbit_->distance() : 3.2f;
}

void Scene3dGpuPresent::remember_view_size(int width_px, int height_px) const {
  if (orbit_) {
    const_cast<OrbitFrame*>(orbit_)->remember_view_size(width_px, height_px);
  }
}

Extent2 Scene3dGpuPresent::world_extent() const {
  // DEM drape is the China raster. A 2D extent in pixels, or a world box,
  // normalizes that raster into a sticker on a huge ocean (flash-correct,
  // then ??). Only a lon/lat box inside China may reframe the orbit.
  if (orbit_) {
    const Extent2 e = orbit_->world_extent();
    if (extent_looks_like_china(e)) {
      return e;
    }
  }
  return kChinaLonLatExtent;
}

render::rhi::CameraMatrices Scene3dGpuPresent::camera_matrices(
    float aspect) const {
  if (!orbit_) {
    // Never hand FlyCube an identity view/proj (default CameraMatrices)  - the
    // globe at R≈2 is clipped and HWND capture shows only clear-color sky.
    static int once = 0;
    if (once < 2) {
      ++once;
      std::fprintf(stderr,
                   "scene3d.present: orbit_ unset  - using fallback orbit "
                   "camera (bind_orbit missing)\n");
    }
    const float asp = aspect > 0.05f ? aspect : 1.333f;
    float dist = distance() > 0.15f ? distance() : 0.15f;
    if (dist < kScene3dDollyMinLocal) {
      dist = kScene3dDollyMinLocal;
    }
    return render::rhi::make_orbit_camera(yaw(), pitch(), dist, kScene3dFovY,
                                          asp, 0.1f, 100.f);
  }
  return orbit_->camera_matrices(aspect);
}

render::rhi::CameraMatrices Scene3dGpuPresent::camera_matrices_ortho(
    float width_px, float height_px) const {
  if (!orbit_) {
    return {};
  }
  return orbit_->camera_matrices_ortho(width_px, height_px);
}

void Scene3dGpuPresent::abandon(AtmosphereSession* atmosphere) {
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  if (atmosphere) {
    atmosphere->release_passes();
  }
  mesh_device_ = nullptr;
  dem_gpu_synced_after_ocean_ = false;
  dem_gpu_synced_after_sky_ = false;
  // FlyCube / MapViewport may have shut down (or leaked) the Device already.
  // release() would destroy_pipeline/buffer on a dangling Device* - AV on
  // self-test teardown. Match WorldPass::~WorldPass and drop handles only.
  gpu_scene_.abandon();
  hud_pass_.abandon_gpu();
  shell_overlay_.abandon_gpu();
  prep_mailbox_.invalidate();
  if (prep_staging_) {
    std::lock_guard<std::mutex> staging_lock(prep_staging_->mu);
    prep_staging_->blob.reset();
  }
}

bool Scene3dGpuPresent::needs_local_mesh_rebuild_locked() const {
  const Extent2 extent = world_extent();
  const float orbit_distance = distance();
  if (dem_local_xyz_count_ > 0 && dem_local_idx_count_ > 0 &&
      dem_local_xyz_count_ <= local_xyz_.size() &&
      dem_local_idx_count_ <= local_idx_.size()) {
    double box_minx = extent.xmin;
    double box_miny = extent.ymin;
    double box_maxx = extent.xmax;
    double box_maxy = extent.ymax;
    vista::dem_seed_lonlat_box(extent.xmin, extent.ymin, extent.xmax,
                               extent.ymax, &box_minx, &box_miny, &box_maxx,
                               &box_maxy);
    const int want_key = vista::dem_seed_cache_key(orbit_distance);
    if (terrain_lod_edge_ == want_key &&
        geo_frame_.as_vista().matches_lonlat(box_minx, box_miny, box_maxx,
                                             box_maxy)) {
      return false;
    }
  }
  return true;
}

void Scene3dGpuPresent::apply_async_prep_staging_locked(
    uint64_t published_gen) {
  if (published_gen == 0 || !prep_staging_) {
    return;
  }
  std::shared_ptr<PrepStagingBlob> blob;
  {
    std::lock_guard<std::mutex> lock(prep_staging_->mu);
    blob = std::move(prep_staging_->blob);
    prep_staging_->blob.reset();
  }
  if (!blob || blob->gen != published_gen || blob->xyz.empty() ||
      blob->world.node_count() == 0) {
    return;
  }
  terrain_world_ = std::move(blob->world);
  local_xyz_.swap(blob->xyz);
  local_idx_.swap(blob->idx);
  geo_frame_ = blob->geo;
  terrain_lod_edge_ = blob->lod_edge;
  dem_local_xyz_count_ = local_xyz_.size();
  dem_local_idx_count_ = local_idx_.size();
}

void Scene3dGpuPresent::post_async_cold_prep_locked() {
  // CPU-only remesh on executor; Display keeps prior mesh for this present.
  // Atmosphere prepare_for_present (POD) stays on Display — residual T2.1.
  // Worker captures shared hub + live_gen only (no Scene3dGpuPresent*).
  const Extent2 extent = world_extent();
  const float orbit_distance = distance();
  const OrbitGeoFrame geo = geo_frame_;
  const int lod = terrain_lod_edge_;
  const GisScene* scene = scene_;
  std::vector<float> seed_xyz;
  std::vector<unsigned> seed_idx;
  if (dem_local_xyz_count_ > 0 && dem_local_xyz_count_ <= local_xyz_.size()) {
    seed_xyz.assign(
        local_xyz_.begin(),
        local_xyz_.begin() +
            static_cast<std::ptrdiff_t>(dem_local_xyz_count_));
  }
  if (dem_local_idx_count_ > 0 && dem_local_idx_count_ <= local_idx_.size()) {
    seed_idx.assign(
        local_idx_.begin(),
        local_idx_.begin() +
            static_cast<std::ptrdiff_t>(dem_local_idx_count_));
  }
  auto hub = prep_staging_;
  auto live_gen = prep_mailbox_.live_gen_atomic();
  (void)prep_mailbox_.stage_async(
      detail::Scene3dPrepAction::kColdRebuild,
      [hub, live_gen, extent, orbit_distance, geo, lod, scene,
       seed_xyz = std::move(seed_xyz),
       seed_idx = std::move(seed_idx)](uint64_t prep_gen) mutable {
        std::vector<float> xyz = std::move(seed_xyz);
        std::vector<unsigned> idx = std::move(seed_idx);
        vista::World staging_world;
        OrbitGeoFrame staging_geo = geo;
        int staging_lod = lod;
        rebuild_terrain_mesh(&staging_world, scene, extent, orbit_distance,
                             &xyz, &idx, &staging_geo, &staging_lod);
        if (!live_gen ||
            live_gen->load(std::memory_order_acquire) > prep_gen) {
          return;
        }
        auto blob = std::make_shared<PrepStagingBlob>();
        blob->gen = prep_gen;
        blob->world = std::move(staging_world);
        blob->xyz = std::move(xyz);
        blob->idx = std::move(idx);
        blob->geo = staging_geo;
        blob->lod_edge = staging_lod;
        if (!hub) {
          return;
        }
        {
          std::lock_guard<std::mutex> lock(hub->mu);
          hub->blob = std::move(blob);
        }
      });
}

bool Scene3dGpuPresent::rebuild_local_mesh() {
  // Caller must hold present_mu_ (present / paint).
  BASE_TRACE_EVENT("mesh", "scene3d.mesh");
  const Extent2 extent = world_extent();
  const float orbit_distance = distance();
  if (!needs_local_mesh_rebuild_locked()) {
    return false;
  }
  // Build into fresh locals then swap  - same pattern as WorldPass::sync_from.
  // In-place push_back on member local_idx_ AVd in Debug STL _Orphan_all under
  // world3d showcase (cdb: rebuild_terrain_mesh -> vector::_Change_array).
  std::vector<float> xyz;
  std::vector<unsigned> idx;
  if (dem_local_xyz_count_ > 0 && dem_local_xyz_count_ <= local_xyz_.size()) {
    xyz.assign(local_xyz_.begin(),
               local_xyz_.begin() +
                   static_cast<std::ptrdiff_t>(dem_local_xyz_count_));
  }
  if (dem_local_idx_count_ > 0 && dem_local_idx_count_ <= local_idx_.size()) {
    idx.assign(local_idx_.begin(),
               local_idx_.begin() +
                   static_cast<std::ptrdiff_t>(dem_local_idx_count_));
  }

  const size_t xyz_before = xyz.size();
  const size_t idx_before = idx.size();
  const int lod_before = terrain_lod_edge_;
  OrbitGeoFrame geo = geo_frame_;
  int lod = terrain_lod_edge_;
  rebuild_terrain_mesh(&terrain_world_, scene_, extent, orbit_distance, &xyz,
                       &idx, &geo, &lod);
  geo_frame_ = geo;
  terrain_lod_edge_ = lod;
  dem_local_xyz_count_ = xyz.size();
  dem_local_idx_count_ = idx.size();
  local_xyz_.swap(xyz);
  local_idx_.swap(idx);
  return local_xyz_.size() != xyz_before || local_idx_.size() != idx_before ||
         terrain_lod_edge_ != lod_before;
}

bool Scene3dGpuPresent::present(render::rhi::Device* device, uint32_t width_px,
                                uint32_t height_px,
                                const ui::gfx::ShellRaster* shell,
                                uint64_t shell_generation,
                                AtmosphereSession& atmosphere) {
  BASE_TRACE_EVENT("present", "scene3d.present");
  if (!device || width_px == 0 || height_px == 0) {
    LOGGING(LOG_ERROR, "scene3d.present fail: bad args device=%p size=%ux%u",
            device, width_px, height_px);
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(present_mu_);
  remember_view_size(static_cast<int>(width_px), static_cast<int>(height_px));
  int64_t prep_overlap_ms = 0;
  detail::Scene3dPrepAction drained_prep =
      detail::Scene3dPrepAction::kWarmReuse;
  bool applied_async_prep = false;
  if (prep_mailbox_.drain(/*wait_if_empty=*/false, &drained_prep,
                          &prep_overlap_ms)) {
    if (drained_prep == detail::Scene3dPrepAction::kColdRebuild) {
      apply_async_prep_staging_locked(prep_mailbox_.published_gen());
      applied_async_prep = true;
    }
  }
  // Globe path draws DEM on the UV sphere in AtmosphereFrame -- skip flat
  // terrain rebuild/sync. Rebuilding china_dem into WorldPass then swapping
  // Debug STL instances AVd under Null showcase (cdb: WorldPass::sync_from).
  const bool globe_on = atmosphere.globe_enabled();
  bool dem_rebuilt = applied_async_prep;
  bool defer_cold_prep_async = false;
  if (!globe_on) {
    base::ElapsedTimer mesh_timer;
    const bool need_cold = needs_local_mesh_rebuild_locked();
    const bool has_drawable =
        dem_local_xyz_count_ > 0 && terrain_world_.node_count() > 0;
    if (need_cold && !has_drawable) {
      // First cold frame: sync remesh on Display (no prior mesh to draw).
      dem_rebuilt = rebuild_local_mesh() || dem_rebuilt;
      note_scene3d_phase_mesh(static_cast<int64_t>(
          mesh_timer.elapsed_milliseconds() + 0.5));
    } else if (need_cold && has_drawable) {
      // T2.1: orbit with prior mesh; post CPU remesh after graph::present.
      defer_cold_prep_async = true;
      note_scene3d_phase_mesh(0);
    } else {
      note_scene3d_phase_mesh(static_cast<int64_t>(
          mesh_timer.elapsed_milliseconds() + 0.5));
    }
    // Contour suite / carto overrides land on china_dem tiles. Must run after
    // rebuild (seed hypsometric/map-drape) and before overlay_tin attach.
    apply_dem_drape_locked();
    attach_overlay_tin_locked(dem_rebuilt || overlays_.tin_dirty());
    attach_overlay_pointcloud_locked(dem_rebuilt ||
                                     overlays_.pointcloud_dirty());
    // 3D Tiles product stream: select -> LRU ensure under session defaults
    // (kDefaultMaxTiles / kDefaultMaxEnsure / 8 MiB cache).
    if (TilesetStreamSession* stream = live_tileset_stream_locked()) {
      if (stream->active()) {
        stream->pump_product(&terrain_world_, orbit_);
        gpu_scene_.set_tileset_content_cache(&stream->cache());
      }
    }
  } else {
    note_scene3d_phase_mesh(0);
  }
  if (terrain_world_.node_count() == 0 && !globe_on) {
    LOGGING(LOG_ERROR,
            "scene3d.present fail: empty terrain mesh (no DEM / extent). "
            "size=%ux%u orbit_dist=%.3f",
            width_px, height_px, distance());
    return false;
  }
  if (mesh_device_ != device) {
    atmosphere.release_passes();
    gpu_scene_.abandon();
    mesh_device_ = device;
    dem_gpu_synced_after_ocean_ = false;
    dem_gpu_synced_after_sky_ = false;
    gpu_scene_.clear_solid_terrain_cache();
    prep_mailbox_.invalidate();
    if (prep_staging_) {
      std::lock_guard<std::mutex> staging_lock(prep_staging_->mu);
      prep_staging_->blob.reset();
    }
  }
  // Cold ocean: defer WorldPass::sync_from until after ocean height alloc so
  // the first (and only) instance copy + meshes_dirty lands after FlyCube
  // heap settles  - avoids a throwaway sync before prepare_gpu.
  const bool skip_ocean_early = []() {
    if (const char* e = base::switch_cstr("atmosphere-skip-ocean")) {
      return e[0] == '1' && e[1] == '\0';
    }
    return false;
  }();
  const vista::atmosphere::Environment* env_early = atmosphere.environment();
  // Keep early sync when overlay TIN / studio_block need live instances before
  // ocean prepare (mine/stormsurge). Plain DEM cold can defer.
  const bool cold_ocean_defer_sync =
      !globe_on && env_early && env_early->ocean_enabled() &&
      !skip_ocean_early && !dem_gpu_synced_after_ocean_ &&
      !overlays_.tin_has_albedo() && !studio_block_;
  base::ElapsedTimer sync_timer;
  if (globe_on) {
    // Drop any leftover flat DEM from a prior non-globe present.
    if (gpu_scene_.instance_count() > 0) {
      gpu_scene_.abandon();
    }
    note_scene3d_phase_sync(0);
  } else if (!cold_ocean_defer_sync) {
    gpu_scene_.sync_from(terrain_world_);
    // Belt-and-suspenders: warm present must never draw with empty GPU instances
    // while terrain_world_ still holds DEM nodes (rebuild_meshes instances=0 ->
    // execute fail after present-warm).
    if (gpu_scene_.instance_count() == 0 && terrain_world_.node_count() > 0) {
      LOGGING(LOG_WARNING,
              "scene3d.present: forced WorldPass resync "
              "(instances=0 nodes=%zu world_gen=%llu synced_gen=%llu)",
              terrain_world_.node_count(),
              static_cast<unsigned long long>(terrain_world_.generation()),
              static_cast<unsigned long long>(gpu_scene_.synced_generation()));
      gpu_scene_.sync_from(terrain_world_);
    }
    note_scene3d_phase_sync(static_cast<int64_t>(
        sync_timer.elapsed_milliseconds() + 0.5));
  }
  // White tint: draped hypsometric / china_rs keep authored RGB (olive
  // multiply made land read as flat mud under FlyCube textured PS).
  gpu_scene_.set_solid_color(1.f, 1.f, 1.f, 1.f);
  // Overlay TIN: solid albedo as instance paint so FlyCube reads purple/cyan
  // even when 2x2 terrain.rgba upload/sample fails (mine/stormsurge slabs).
  if (overlays_.tin_has_albedo()) {
    const uint8_t* tin_albedo = overlays_.tin_albedo();
    uint64_t overlay_id = 0;
    for (size_t ni = 0; ni < terrain_world_.node_count(); ++ni) {
      const vista::Node* n = terrain_world_.node_at(ni);
      if (n && n->kind == vista::NodeKind::kTerrain && n->name == "overlay_tin") {
        overlay_id = n->id;
        break;
      }
    }
    // Isolated geological / hex block: drop china_dem apron so the overlay
    // volume owns the frame (mine cutaway + orthogrid3d).
    if (studio_block_ && overlay_id != 0) {
      std::vector<uint64_t> drop;
      for (size_t ni = 0; ni < terrain_world_.node_count(); ++ni) {
        const vista::Node* n = terrain_world_.node_at(ni);
        if (n && n->kind == vista::NodeKind::kTerrain && n->id != overlay_id) {
          drop.push_back(n->id);
        }
      }
      for (uint64_t id : drop) {
        (void)terrain_world_.remove_node(id);
      }
      if (!drop.empty()) {
        gpu_scene_.sync_from(terrain_world_);
      }
      gis::style::ResolvedPaint bg;
      bg.type = gis::style::LayerType::kBackground;
      bg.fill_color = 0xFFF5F0E6u;
      bg.fill_opacity = 1.f;
      bg.background_color = 0xFFF5F0E6u;
      bg.background_opacity = 1.f;
      gpu_scene_.set_background_paint(bg);
      atmosphere.frame().set_clear_rgb(0.96f, 0.94f, 0.90f);
    }
    for (size_t i = 0; i < gpu_scene_.instance_count(); ++i) {
      const vista::Instance* inst = gpu_scene_.instance_at(i);
      if (!inst || inst->kind != vista::NodeKind::kTerrain) {
        continue;
      }
      const bool by_id = overlay_id != 0 && inst->node_id == overlay_id;
      const bool by_tex =
          inst->terrain.tex_w == 2 && inst->terrain.tex_h == 2 &&
          inst->terrain.rgba.size() >= 16 &&
          inst->terrain.rgba[0] == tin_albedo[0] &&
          inst->terrain.rgba[1] == tin_albedo[1] &&
          inst->terrain.rgba[2] == tin_albedo[2];
      if (!by_id && !by_tex) {
        continue;
      }
      if (inst->has_paint) {
        break;
      }
      // Zone / lithology atlases must stay textured 鈥?solid albedo paint
      // collapses orthogrid3d to one amber parallelogram (color_buckets=2).
      // Storm-surge cyan still needs the solid fill: the depth atlas often
      // samples as DEM green, so the free surface never clears water_on_land.
      const bool water_albedo = tin_albedo[2] >= 140 && tin_albedo[1] >= 100 &&
                                tin_albedo[0] < 90 &&
                                tin_albedo[2] > tin_albedo[0] + 40;
      if (overlays_.tin_has_drape() && !water_albedo) {
        break;
      }
      gis::style::ResolvedPaint fill;
      fill.type = gis::style::LayerType::kFill;
      // Opaque ARGB - avoid double-multiplying alpha via fill_opacity.
      fill.fill_color = 0xFF000000u |
                         (static_cast<uint32_t>(tin_albedo[0]) << 16) |
                         (static_cast<uint32_t>(tin_albedo[1]) << 8) |
                         static_cast<uint32_t>(tin_albedo[2]);
      fill.fill_opacity = 1.f;
      (void)gpu_scene_.set_instance_paint(i, fill);
      LOGGING(LOG_INFO,
              "scene3d.present overlay_tin paint id=%llu inst=%zu by_id=%d "
              "by_tex=%d rgba=%u,%u,%u,%u",
              static_cast<unsigned long long>(overlay_id), i, by_id ? 1 : 0,
              by_tex ? 1 : 0, tin_albedo[0], tin_albedo[1], tin_albedo[2],
              tin_albedo[3]);
      break;
    }
  }
  gpu_scene_.set_wireframe(wireframe_enabled_);
  if (look_preset_ == Scene3dLookPreset::kLegacyStereo) {
    // Leftover stereo: black clear (sky) + hypsometric DEM; ocean pass supplies
    // the light-blue sea plane when enabled.
    gis::style::ResolvedPaint bg;
    bg.type = gis::style::LayerType::kBackground;
    bg.fill_color = 0xFF000000u;
    bg.fill_opacity = 1.f;
    bg.background_color = 0xFF000000u;
    bg.background_opacity = 1.f;
    gpu_scene_.set_background_paint(bg);
    atmosphere.frame().set_clear_rgb(0.f, 0.f, 0.f);
  } else if (!studio_block_) {
    gpu_scene_.clear_background_paint();
    atmosphere.frame().clear_clear_rgb();
  } else {
    // Keep cream studio clear set above when overlay owns the frame.
    gis::style::ResolvedPaint bg;
    bg.type = gis::style::LayerType::kBackground;
    bg.fill_color = 0xFFF5F0E6u;
    bg.fill_opacity = 1.f;
    bg.background_color = 0xFFF5F0E6u;
    bg.background_opacity = 1.f;
    gpu_scene_.set_background_paint(bg);
    atmosphere.frame().set_clear_rgb(0.96f, 0.94f, 0.90f);
  }
  // T2.0 warm / sync cold: stage before Display POD prep. Deferred async cold
  // stages after graph::present so L2 overlaps the next Display present.
  if (!defer_cold_prep_async) {
    const detail::Scene3dPrepAction staged_prep =
        dem_rebuilt ? detail::Scene3dPrepAction::kColdRebuild
                    : detail::Scene3dPrepAction::kWarmReuse;
    (void)prep_mailbox_.stage(staged_prep);
  }
  // Atmosphere POD + cold ocean height before diagnostics / HUD so CPU prep
  // is not stranded behind AABB scans or GDI HUD raster, and GPU present is
  // not waiting on that work. prepare_for_present stays POD-only (no
  // OceanPass::prepare_gpu); warm frames skip prepare_gpu entirely.
  const vista::atmosphere::Environment* env = atmosphere.environment();
  const bool ocean_on = !globe_on && env && env->ocean_enabled();
  const bool cloud_on = !globe_on && env && env->cloud_enabled();
  const bool sat_cloud_on = globe_on && atmosphere.sat_cloud_enabled();
  const bool sky_on = [&]() {
    if (globe_on) {
      return true;
    }
    if (!env || !env->sky_enabled()) {
      return false;
    }
    if (const char* e = base::switch_cstr("atmosphere-skip-sky")) {
      if (e[0] == '1' && e[1] == '\0') {
        return false;
      }
    }
    return true;
  }();
  const bool fog_on = env && env->fog_enabled();
  // Record order (ocean SRV0 must not precede textured DEM):
  //   sky (+ depth clear) -> opaque DEM -> ocean -> cloud/fog
  // Globe path: sky -> globe DEM -> sat clouds (+ fog); skip flat DEM/ocean.
  atmosphere.frame().set_ocean_enabled(false);
  atmosphere.frame().set_cloud_enabled(false);
  atmosphere.frame().set_sky_enabled(sky_on);
  atmosphere.frame().set_fog_enabled(false);
  atmosphere.frame().set_globe_enabled(globe_on);
  atmosphere.frame().set_sat_cloud_enabled(false);
  if (!atmosphere.prepare_for_present()) {
    LOGGING(LOG_ERROR, "scene3d.present fail: atmosphere.prepare_for_present");
    return false;
  }
  // Optional isolate: ATMOSPHERE_SKIP_OCEAN=1 keeps sky/DEM without ocean
  // (debug atmosphere.full near-black China). Skip prepare_gpu too — height
  // texture alloc still recycles FlyCube SRVs and blacks DEM albedo.
  const bool skip_ocean = skip_ocean_early;
  // Usable DEM bake (hypsometric / drape) on CPU — prefer textured draw over
  // the olive solid gate. Solid + skip-ocean was a FlyCube SRV black-out
  // score face; with a healthy bake we remesh after ocean height instead.
  bool dem_bake_usable = false;
  {
    size_t tex_nodes = 0;
    unsigned tex_mean = 0;
    for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
      const vista::Node* n = terrain_world_.node_at(i);
      if (!n || n->terrain.rgba.empty() || n->terrain.tex_w == 0 ||
          n->terrain.tex_h == 0) {
        continue;
      }
      ++tex_nodes;
      const auto& rgba = n->terrain.rgba;
      const size_t step = (std::max)(size_t{4}, rgba.size() / 64);
      unsigned acc = 0;
      size_t samples = 0;
      for (size_t o = 0; o + 2 < rgba.size(); o += step) {
        acc += rgba[o] + rgba[o + 1] + rgba[o + 2];
        ++samples;
      }
      if (samples > 0) {
        tex_mean = acc / (samples * 3);
      }
    }
    // Near-black SRV failure modes sit well below this floor (tex_mean~0–8).
    dem_bake_usable = tex_nodes > 0 && tex_mean >= 16u;
  }
  // Only skip ocean when there is no bake to remesh after height alloc.
  const bool skip_ocean_for_dem_albedo =
      ocean_on && !skip_ocean &&
      look_preset_ != Scene3dLookPreset::kLegacyStereo && !dem_bake_usable;
  // Olive solid gate when bake is missing/unusable, or on the cold ocean
  // height-alloc frame only: FlyCube recycles the DEM albedo SRV during
  // prepare_gpu (alpha 0) even when the CPU bake looks healthy. Warm frames
  // skip prepare_gpu and must keep textured DEM — forcing the gate for every
  // ocean_on frame left China olive forever and (with skip below) never drew
  // the sea plane (ocean_draw=0 bake=1 albedo_gate=1).
  // atmosphere.full landish score needs solid PSO + olive ColorCB on cold
  // churn only (see TerrainPass / draw.cc).
  const bool flycube_ocean_churn =
      ocean_on && !dem_gpu_synced_after_ocean_;
  const bool need_albedo_gate =
      (!dem_bake_usable || flycube_ocean_churn) &&
      (sky_on || ocean_on ||
       look_preset_ == Scene3dLookPreset::kLegacyStereo);
  base::ElapsedTimer ocean_prep_timer;
  // Cold path only: allocate ocean height before DEM remesh so FlyCube does
  // not recycle hypsometric albedo as the height map. Warm frames (already
  // synced after ocean) skip prepare_gpu — OceanPass::record does one
  // Gerstner/in-place height upload instead of prepare_gpu + record double work.
  const bool need_ocean_height_before_dem =
      ocean_on && !skip_ocean && !dem_gpu_synced_after_ocean_ &&
      !skip_ocean_for_dem_albedo;
  if (need_ocean_height_before_dem &&
      !atmosphere.ocean_pass().prepare_gpu(device)) {
    LOGGING(LOG_ERROR, "scene3d.present fail: ocean.prepare_gpu");
    return false;
  }
  note_scene3d_phase_ocean_prep(static_cast<int64_t>(
      ocean_prep_timer.elapsed_milliseconds() + 0.5));
  // Sync after ocean height so albedo uploads see the settled FlyCube heap.
  // When cold_ocean_defer_sync without ocean churn, still perform the deferred
  // first sync so GPU instances match terrain_world_.
  if (need_ocean_height_before_dem || cold_ocean_defer_sync) {
    base::ElapsedTimer post_ocean_sync;
    gpu_scene_.sync_from(terrain_world_);
    if (cold_ocean_defer_sync) {
      note_scene3d_phase_sync(static_cast<int64_t>(
          post_ocean_sync.elapsed_milliseconds() + 0.5));
    }
  }
  // One cold DEM remesh after ocean height / first sky depth. Warm StaticReuse
  // leaves meshes_dirty_ false so rebuild_count stays 0. Globe DEM is spherical
  // and must not dirty the flat mesh. cold_ocean_defer_sync already set dirty
  // via sync_from; mark only when an earlier sync uploaded before ocean height.
  const bool cold_dem_remesh =
      !globe_on &&
      ((need_ocean_height_before_dem && !cold_ocean_defer_sync) ||
       (sky_on && !dem_gpu_synced_after_sky_));
  if (cold_dem_remesh) {
    gpu_scene_.mark_meshes_dirty();
  }
  // Drive DEM Lambert from atmosphere sun after prepare_for_present so
  // advance_sim_time updates are visible this frame.
  {
    // Start from program defaults so a missing Environment never zero-lights
    // untextured meshes into pure-black slabs.
    render::programs::Light light;
    if (const vista::atmosphere::Environment* env_light =
            atmosphere.environment()) {
      const float az = env_light->params().sun_azimuth_rad;
      const float el = env_light->params().sun_elevation_rad;
      const float cos_el = std::cos(el);
      light.dir_x = std::cos(az) * cos_el;
      light.dir_y = -std::sin(el);
      light.dir_z = std::sin(az) * cos_el;
      if (look_preset_ == Scene3dLookPreset::kLegacyStereo) {
        // Leftover stereo: harder key light on vertical DEM faces.
        light.ambient = 0.34f;
        light.intensity = 1.55f;
        light.color_r = 1.f;
        light.color_g = 0.96f;
        light.color_b = 0.90f;
      } else {
        // Balanced fill: soft enough for landish greens under sky, firm
        // enough that DEM relief still reads (0.72 washed facets flat).
        light.ambient = 0.52f;
        light.intensity = 1.28f;
        light.color_r = 1.f;
        light.color_g = 0.97f;
        light.color_b = 0.92f;
      }
    }
    gpu_scene_.set_light(light);
  }
  // White tint first. Legacy stereo may force solid olive via update_solid.
  gpu_scene_.set_solid_color(1.f, 1.f, 1.f, 1.f);
  gpu_scene_.update_solid_terrain(terrain_world_.generation(),
                                  need_albedo_gate);
  // Skip ocean draw only when DEM bake is unusable. The solid albedo gate
  // protects textured DEM during cold height alloc; it must not suppress the
  // sea plane — (need_albedo_gate && ocean_on) was always true under the old
  // flycube_ocean_churn=ocean_on rule and forced ocean_draw=0 forever.
  const bool skip_ocean_for_solid_gate = skip_ocean_for_dem_albedo;

  // Pipelines before HUD fill / graph::present. Mesh upload stays inside
  // record_draws so sky/depth allocation in the pre slot happens first.
  base::ElapsedTimer pso_timer;
  if (!gpu_scene_.warm_pipelines(device)) {
    LOGGING(LOG_ERROR, "scene3d.present fail: warm_pipelines");
    return false;
  }
  note_scene3d_phase_pso(static_cast<int64_t>(
      pso_timer.elapsed_milliseconds() + 0.5));

  // Verbose DEM / AABB dump once (or when SCENE3D_PRESENT_LOG=1). Scanning
  // every vertex each frame was a measurable FPS tax in Debug builds — keep
  // this after cold ocean/DEM prep so it never delays height alloc.
  {
    static bool logged_once = false;
    const bool force_log = []() {
      if (const char* e = base::switch_cstr("scene3d-present-log")) {
        return e[0] == '1' && e[1] == '\0';
      }
      return false;
    }();
    if (force_log || !logged_once) {
      logged_once = true;
      size_t tex_nodes = 0;
      for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
        const vista::Node* n = terrain_world_.node_at(i);
        if (n && !n->terrain.rgba.empty() && n->terrain.tex_w > 0 &&
            n->terrain.tex_h > 0) {
          ++tex_nodes;
        }
      }
      unsigned alpha_mean = 0;
      unsigned landish = 0;
      unsigned samples = 0;
      for (size_t ni = 0; ni < terrain_world_.node_count(); ++ni) {
        const vista::Node* n = terrain_world_.node_at(ni);
        if (!n || n->terrain.rgba.size() < 4) {
          continue;
        }
        const auto& rgba = n->terrain.rgba;
        const size_t step = (std::max)(size_t{4}, rgba.size() / 256);
        for (size_t o = 0; o + 3 < rgba.size(); o += step) {
          alpha_mean += rgba[o + 3];
          if (rgba[o + 1] > rgba[o] + 8 && rgba[o + 1] > 40) {
            ++landish;
          }
          ++samples;
        }
      }
      if (samples > 0) {
        alpha_mean /= samples;
      }
      LOGGING(LOG_INFO,
              "scene3d.present dem nodes=%zu textured=%zu size=%ux%u "
              "yaw=%.2f pitch=%.2f dist=%.2f alpha_mean=%u landish=%u/%u",
              terrain_world_.node_count(), tex_nodes, width_px, height_px,
              yaw(), pitch(), distance(), alpha_mean, landish, samples);
      if (!local_xyz_.empty() && local_xyz_.size() >= 3) {
        float mn_x = local_xyz_[0];
        float mn_y = local_xyz_[1];
        float mn_z = local_xyz_[2];
        float mx_x = mn_x;
        float mx_y = mn_y;
        float mx_z = mn_z;
        for (size_t i = 0; i + 2 < local_xyz_.size(); i += 3) {
          mn_x = (std::min)(mn_x, local_xyz_[i]);
          mn_y = (std::min)(mn_y, local_xyz_[i + 1]);
          mn_z = (std::min)(mn_z, local_xyz_[i + 2]);
          mx_x = (std::max)(mx_x, local_xyz_[i]);
          mx_y = (std::max)(mx_y, local_xyz_[i + 1]);
          mx_z = (std::max)(mx_z, local_xyz_[i + 2]);
        }
        LOGGING(LOG_INFO,
                "scene3d.present mesh_aabb x=[%.2f,%.2f] y=[%.2f,%.2f] "
                "z=[%.2f,%.2f] verts=%zu",
                mn_x, mx_x, mn_y, mx_y, mn_z, mx_z, local_xyz_.size() / 3);
      }
    }
  }
  float aspect = static_cast<float>(width_px) /
                 static_cast<float>(height_px > 0 ? height_px : 1);
  const render::rhi::CameraMatrices cam = camera_matrices(aspect);
  const render::rhi::CameraMatrices* view_camera = nullptr;
  if (device->backend() != render::rhi::Backend::kNull) {
    view_camera = &cam;
  } else {
    gpu_scene_.clear_view_camera();
  }
  {
    static int cam_log = 0;
    if (cam_log < 3) {
      ++cam_log;
      std::fprintf(
          stderr,
          "scene3d.present camera kind=%d view00=%.3f view_t=(%.3f,%.3f,%.3f) "
          "proj00=%.3f dist=%.3f yaw=%.2f pitch=%.2f aspect=%.3f orbit=%p\n",
          static_cast<int>(cam.kind), cam.view[0], cam.view[12], cam.view[13],
          cam.view[14], cam.proj[0], distance(), yaw(), pitch(), aspect,
          static_cast<const void*>(orbit_));
    }
  }
  render_engine_name = render::rhi::backend_display_name(device->backend());
  note_present_frame();

  const int cloud_quality = (env && cloud_on) ? env->params().quality : 1;
  vista::OpaqueEffect opaque(&gpu_scene_);
  // Views shell is painted on the parent widget. A fullscreen shell quad on
  // the swapchain covers terrain — do not composite DrawRequest.shell here.
  (void)shell;
  (void)shell_generation;
  shell_overlay_.clear();

  // Screen HUD (compass / status / place-names / wind / engine badge) via
  // vista HudPass GPU overlay — not HDC soft paint / FORCE_GDI. After PSO warm
  // so HUD CPU fill does not serialize ahead of pipeline compile.
  ++hud_generation_;
  vista::HudFrame hud_frame;
  fill_scene3d_hud_frame(&hud_frame, this, atmosphere, width_px, height_px,
                         hud_generation_);
  hud_pass_.bind(std::move(hud_frame));
  vista::HudEffect hud_effect(&hud_pass_);

  const bool need_rebuild =
      !globe_on &&
      gpu_scene_.needs_mesh_upload(device, width_px, height_px);
  // rebuild_count is the warm/cold signal; upload/rebuild ms come from
  // WorldPass::last_rebuild_ms after graph::present (record_draws).

  const bool skip_post = []() {
    if (const char* e = base::switch_cstr("atmosphere-skip-post")) {
      return e[0] == '1' && e[1] == '\0';
    }
    return false;
  }();
  // Pre slot keeps flat ocean off inside record_pre_opaque. Post draws it
  // before cloud/fog/sat when this flag is on.
  const bool ocean_draw =
      ocean_on && !skip_ocean && !skip_ocean_for_solid_gate;
  atmosphere.frame().set_ocean_enabled(ocean_draw);
  {
    // One-shot: Hs must stay well below DEM peak (~0.14) or ocean hides land.
    static bool ocean_diag_once = false;
    if (!ocean_diag_once && ocean_on) {
      ocean_diag_once = true;
      const auto& op = atmosphere.ocean_pass();
      LOGGING(LOG_INFO,
              "scene3d.present ocean_draw=%d bake=%d albedo_gate=%d "
              "Hs=%.3f mask_mean=%.3f idx=%u",
              ocean_draw ? 1 : 0, dem_bake_usable ? 1 : 0,
              need_albedo_gate ? 1 : 0, op.params().significant_wave_height,
              op.sea_mask_mean(), op.index_count());
    }
  }
  if (!skip_post) {
    atmosphere.frame().set_cloud_enabled(cloud_on);
    atmosphere.frame().set_fog_enabled(fog_on);
    atmosphere.frame().set_sat_cloud_enabled(sat_cloud_on);
  }

  vista::AtmosphereEffects atmosphere_effects(&atmosphere.frame(),
                                                           cloud_quality);
  render::graph::ViewInput view_input;
  view_input.width_px = width_px;
  view_input.height_px = height_px;
  view_input.camera = view_camera;
  view_input.effects.push_back(atmosphere_effects.pre_effect());
  // Globe DEM is in the pre slot. Flat DEM must not enter the list.
  if (!globe_on) {
    view_input.effects.push_back(&opaque);
  }
  view_input.effects.push_back(atmosphere_effects.post_effect());
  view_input.effects.push_back(&hud_effect);

  // Latch cold-ocean / cold-sky bookkeeping before graph::present. Locals
  // above already captured this frame's churn; delaying the flags until after
  // a successful present left dem_gpu_synced_after_ocean_/sky_ false on every
  // Design B FlyCube fail and forced prepare_gpu + mark_meshes_dirty forever
  // (scene3d.present fail spam + sub-5 FPS with ocean=1).
  if (ocean_on && !skip_ocean) {
    dem_gpu_synced_after_ocean_ = true;
  }
  if (sky_on) {
    dem_gpu_synced_after_sky_ = true;
  }

  base::ElapsedTimer record_timer;
  const bool ok = render::graph::present(device, view_input);
  if (!ok) {
    LOGGING(LOG_ERROR,
            "scene3d.present fail: graph::present size=%ux%u nodes=%zu "
            "backend=%s (cold ocean/sky already latched; next frame warm)",
            width_px, height_px, terrain_world_.node_count(),
            render_engine_name);
    return false;
  }
  const int64_t rebuild_ms = gpu_scene_.last_rebuild_ms();
  note_scene3d_phase_upload(rebuild_ms);
  note_scene3d_phase_rebuild(rebuild_ms, need_rebuild ? 1 : 0);
  note_scene3d_phase_record(static_cast<int64_t>(
      record_timer.elapsed_milliseconds() + 0.5));
  // graph::present owns execute + present. No second clock without a new
  // signature.
  note_scene3d_phase_present(0);
  if (defer_cold_prep_async) {
    post_async_cold_prep_locked();
  } else {
    (void)prep_mailbox_.drain(/*wait_if_empty=*/false, &drained_prep);
  }
  note_scene3d_phase_prep_overlap(prep_overlap_ms);
  return true;
}

}  // namespace content
