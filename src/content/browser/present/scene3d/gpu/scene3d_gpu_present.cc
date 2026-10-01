// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <vector>

#include "base/core/log.h"
#include "base/time/elapsed_timer.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/frame/terrain_mesh.h"
#include "content/browser/present/scene3d/frame/tileset_stream.h"
#include "content/browser/present/scene3d/scene3d_phase_profile.h"
#include "effect/atmosphere/frame/atmosphere_effects.h"
#include "effect/scene/opaque_effect.h"
#include "effect/scene/scene.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"
#include "render/graph/frame_graph.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "base/trace/event/process_trace.h"

namespace content {

void Scene3dGpuPresent::bind_orbit(const OrbitFrame* orbit) {
  orbit_ = orbit;
}

void Scene3dGpuPresent::bind_map(const MapScene* scene) {
  scene_ = scene;
  local_xyz_.clear();
  local_idx_.clear();
  terrain_lod_edge_ = 0;
}

void Scene3dGpuPresent::set_wireframe_enabled(bool on) {
  wireframe_enabled_ = on;
  gpu_scene_.set_wireframe(on);
}

void Scene3dGpuPresent::set_look_preset(Scene3dLookPreset preset) {
  look_preset_ = preset;
  if (preset != Scene3dLookPreset::kLegacyStereo) {
    legacy_labels_.clear();
    legacy_coast_seeded_ = false;
    legacy_overlays_attempted_ = false;
  }
}

bool Scene3dGpuPresent::ensure_legacy_overlays() {
  legacy_overlays_attempted_ = true;
  if (legacy_labels_.empty()) {
    // Major China place-names (leftover stereo label batch character). Lon/lat
    // approximate; elevation is sampled at paint/present via geo frame.
    static const Scene3dLegacyLabel kCities[] = {
        {"乌鲝木齝", 87.62, 43.83}, {"拉蝨", 91.11, 29.97},
        {"西宝", 101.78, 36.62},   {"兰州", 103.83, 36.06},
        {"银川", 106.27, 38.47},   {"呼和浩特", 111.75, 40.84},
        {"哈尔滨", 126.53, 45.80}, {"长春", 125.32, 43.88},
        {"沈阳", 123.43, 41.80},   {"�-京", 116.40, 39.90},
        {"天津", 117.20, 39.08},   {"石家庄", 114.51, 38.04},
        {"太原", 112.55, 37.87},   {"济�-", 117.00, 36.65},
        {"郑州", 113.62, 34.75},   {"西安", 108.94, 34.34},
        {"�-京", 118.78, 32.06},   {"上海", 121.47, 31.23},
        {"杭州", 120.15, 30.28},   {"坈肥", 117.28, 31.86},
        {"禝州", 119.30, 26.08},   {"�-昌", 115.86, 28.68},
        {"武汉", 114.31, 30.57},   {"长沙", 112.98, 28.19},
        {"广州", 113.26, 23.13},   {"�-宝", 108.37, 22.82},
        {"海坣", 110.35, 20.02},   {"戝都", 104.06, 30.67},
        {"針庆", 106.55, 29.56},   {"贵阳", 106.63, 26.65},
        {"昆明", 102.71, 25.04},   {"坰�-", 121.56, 25.04},
    };
    legacy_labels_.assign(std::begin(kCities), std::end(kCities));
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
  std::lock_guard<std::mutex> lock(present_mu_);
  overlay_xyz_geo_.clear();
  overlay_rgba_.clear();
  overlay_pointcloud_dirty_ = true;
  if (!xyz_lon_lat_elev || point_count < 1) {
    return;
  }
  const size_t n = static_cast<size_t>(point_count);
  overlay_xyz_geo_.assign(xyz_lon_lat_elev, xyz_lon_lat_elev + n * 3);
  if (rgba) {
    overlay_rgba_.assign(rgba, rgba + n * 4);
  }
}

void Scene3dGpuPresent::clear_overlay_pointcloud() {
  std::lock_guard<std::mutex> lock(present_mu_);
  overlay_xyz_geo_.clear();
  overlay_rgba_.clear();
  overlay_pointcloud_dirty_ = true;
}

void Scene3dGpuPresent::set_overlay_tin_mesh(const float* xyz_lon_lat_elev,
                                             int point_count,
                                             const unsigned* indices,
                                             int index_count,
                                             const uint8_t* albedo_rgba) {
  std::lock_guard<std::mutex> lock(present_mu_);
  overlay_tin_xyz_geo_.clear();
  overlay_tin_idx_.clear();
  overlay_tin_has_albedo_ = false;
  overlay_tin_dirty_ = true;
  if (!xyz_lon_lat_elev || point_count < 3 || !indices || index_count < 3) {
    return;
  }
  const size_t n = static_cast<size_t>(point_count);
  const size_t ic = static_cast<size_t>(index_count);
  if ((ic % 3u) != 0) {
    return;
  }
  overlay_tin_xyz_geo_.assign(xyz_lon_lat_elev, xyz_lon_lat_elev + n * 3);
  overlay_tin_idx_.assign(indices, indices + ic);
  // Always drape a solid albedo. Untextured kTerrain uses the lit land-green
  // path and reads as near-black slabs under FlyCube (mine / stormsurge).
  if (albedo_rgba) {
    overlay_tin_albedo_[0] = albedo_rgba[0];
    overlay_tin_albedo_[1] = albedo_rgba[1];
    overlay_tin_albedo_[2] = albedo_rgba[2];
    overlay_tin_albedo_[3] = albedo_rgba[3];
  }
  // else: keep member default cyan {46,170,220,230}
  overlay_tin_has_albedo_ = true;
}

void Scene3dGpuPresent::clear_overlay_tin_mesh() {
  std::lock_guard<std::mutex> lock(present_mu_);
  overlay_tin_xyz_geo_.clear();
  overlay_tin_idx_.clear();
  overlay_tin_has_albedo_ = false;
  overlay_tin_dirty_ = true;
}

bool Scene3dGpuPresent::attach_tileset_json(const char* json, size_t len,
                                            const char* name) {
  std::lock_guard<std::mutex> lock(present_mu_);
  if (!tileset_stream_) {
    tileset_stream_ = std::make_unique<TilesetStreamSession>();
  }
  return tileset_stream_->attach_json(&terrain_world_, json, len, name);
}

void Scene3dGpuPresent::set_tileset_content_root(const std::string& root) {
  std::lock_guard<std::mutex> lock(present_mu_);
  if (!tileset_stream_) {
    tileset_stream_ = std::make_unique<TilesetStreamSession>();
  }
  tileset_stream_->set_content_root(root);
}

void Scene3dGpuPresent::clear_tileset() {
  std::lock_guard<std::mutex> lock(present_mu_);
  if (tileset_stream_) {
    tileset_stream_->clear(&terrain_world_);
  }
}

TilesetStreamSession* Scene3dGpuPresent::tileset_stream() {
  return tileset_stream_.get();
}

const TilesetStreamSession* Scene3dGpuPresent::tileset_stream() const {
  return tileset_stream_.get();
}

void Scene3dGpuPresent::attach_overlay_pointcloud_locked(bool force) {
  if (overlay_xyz_geo_.empty() || !geo_frame_.valid) {
    return;
  }
  const size_t n = overlay_xyz_geo_.size() / 3;
  if (n == 0) {
    return;
  }
  if (!force && !overlay_pointcloud_dirty_) {
    for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
      const gis::Node* node = terrain_world_.node_at(i);
      if (node && node->kind == gis::NodeKind::kPointCloud &&
          node->name == "overlay_pointcloud") {
        return;
      }
    }
  }
  // Drop prior overlay nodes (present may early-return mesh rebuild).
  for (size_t i = 0; i < terrain_world_.node_count();) {
    const gis::Node* node = terrain_world_.node_at(i);
    if (node && node->kind == gis::NodeKind::kPointCloud &&
        node->name == "overlay_pointcloud") {
      if (!terrain_world_.remove_node(node->id)) {
        ++i;
      }
      continue;
    }
    ++i;
  }
  std::vector<float> orbit_xyz;
  orbit_xyz.reserve(n * 3);
  float mn_x = 0.f;
  float mn_y = 0.f;
  float mn_z = 0.f;
  float mx_x = 0.f;
  float mx_y = 0.f;
  float mx_z = 0.f;
  for (size_t i = 0; i < n; ++i) {
    const double lon = static_cast<double>(overlay_xyz_geo_[i * 3]);
    const double lat = static_cast<double>(overlay_xyz_geo_[i * 3 + 1]);
    const float elev = overlay_xyz_geo_[i * 3 + 2];
    float ox = 0.f;
    float oy = 0.f;
    float oz = 0.f;
    geo_frame_.lon_lat_to_orbit(lon, lat, elev, &ox, &oy, &oz);
    orbit_xyz.push_back(ox);
    orbit_xyz.push_back(oy);
    orbit_xyz.push_back(oz);
    if (i == 0) {
      mn_x = mx_x = ox;
      mn_y = mx_y = oy;
      mn_z = mx_z = oz;
    } else {
      mn_x = (std::min)(mn_x, ox);
      mn_y = (std::min)(mn_y, oy);
      mn_z = (std::min)(mn_z, oz);
      mx_x = (std::max)(mx_x, ox);
      mx_y = (std::max)(mx_y, oy);
      mx_z = (std::max)(mx_z, oz);
    }
  }
  // Park beads in a thin band above the DEM roof (same framing as overlay TIN).
  float dem_max_y = -1.0e9f;
  const size_t dem_floats = dem_local_xyz_count_;
  for (size_t i = 1; i < dem_floats && i < local_xyz_.size(); i += 3) {
    dem_max_y = (std::max)(dem_max_y, local_xyz_[i]);
  }
  if (!(dem_max_y > -1.0e8f)) {
    dem_max_y = 0.f;
  }
  constexpr float kBeadBase = 0.32f;  // above TIN slab (clearance 0.08 + 0.22)
  constexpr float kBeadSpan = 0.12f;
  const float geo_y0 = mn_y;
  const float geo_span = (std::max)(mx_y - mn_y, 1.0e-4f);
  const float target_base = dem_max_y + kBeadBase;
  for (size_t i = 0; i < n; ++i) {
    const float t = (orbit_xyz[i * 3 + 1] - geo_y0) / geo_span;
    orbit_xyz[i * 3 + 1] = target_base + t * kBeadSpan;
  }
  mn_y = target_base;
  mx_y = target_base + kBeadSpan;
  gis::Node* node = terrain_world_.attach_pointcloud(
      "overlay_pointcloud", mn_x, mn_y, mn_z, mx_x, mx_y, mx_z);
  if (!node) {
    return;
  }
  const uint8_t* rgba =
      overlay_rgba_.size() == n * 4 ? overlay_rgba_.data() : nullptr;
  (void)terrain_world_.set_pointcloud_points(node->id, orbit_xyz.data(), n, rgba,
                                             rgba ? n * 4 : 0);
  overlay_pointcloud_dirty_ = false;
}

void Scene3dGpuPresent::attach_overlay_tin_locked(bool force) {
  if (overlay_tin_xyz_geo_.empty() || overlay_tin_idx_.empty() ||
      !geo_frame_.valid) {
    return;
  }
  const size_t n = overlay_tin_xyz_geo_.size() / 3;
  if (n < 3) {
    return;
  }
  if (!force && !overlay_tin_dirty_) {
    for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
      const gis::Node* node = terrain_world_.node_at(i);
      if (node && node->kind == gis::NodeKind::kTerrain &&
          node->name == "overlay_tin") {
        return;
      }
    }
  }
  // Drop prior overlay tin terrain node (rebuild may leave stale ids).
  for (size_t i = 0; i < terrain_world_.node_count();) {
    const gis::Node* node = terrain_world_.node_at(i);
    if (node && node->kind == gis::NodeKind::kTerrain &&
        node->name == "overlay_tin") {
      if (!terrain_world_.remove_node(node->id)) {
        ++i;
      }
      continue;
    }
    ++i;
  }

  std::vector<float> orbit_xyz;
  orbit_xyz.reserve(n * 3);
  float mn_x = 0.f;
  float mn_y = 0.f;
  float mn_z = 0.f;
  float mx_x = 0.f;
  float mx_y = 0.f;
  float mx_z = 0.f;
  for (size_t i = 0; i < n; ++i) {
    const double lon = static_cast<double>(overlay_tin_xyz_geo_[i * 3]);
    const double lat = static_cast<double>(overlay_tin_xyz_geo_[i * 3 + 1]);
    const float elev = overlay_tin_xyz_geo_[i * 3 + 2];
    float ox = 0.f;
    float oy = 0.f;
    float oz = 0.f;
    geo_frame_.lon_lat_to_orbit(lon, lat, elev, &ox, &oy, &oz);
    orbit_xyz.push_back(ox);
    orbit_xyz.push_back(oy);
    orbit_xyz.push_back(oz);
    if (i == 0) {
      mn_x = mx_x = ox;
      mn_y = mx_y = oy;
      mn_z = mx_z = oz;
    } else {
      mn_x = (std::min)(mn_x, ox);
      mn_y = (std::min)(mn_y, oy);
      mn_z = (std::min)(mn_z, oz);
      mx_x = (std::max)(mx_x, ox);
      mx_y = (std::max)(mx_y, oy);
      mx_z = (std::max)(mx_z, oz);
    }
  }

  std::vector<uint32_t> orbit_idx;
  orbit_idx.reserve(overlay_tin_idx_.size());
  // Reverse winding so FlyCube solid PS does not cull the stratum TIN
  // (point-cloud cubes still drew; single-sided TIN was invisible).
  for (size_t t = 0; t + 2 < overlay_tin_idx_.size(); t += 3) {
    const unsigned a = overlay_tin_idx_[t];
    const unsigned b = overlay_tin_idx_[t + 1];
    const unsigned c = overlay_tin_idx_[t + 2];
    if (a >= n || b >= n || c >= n) {
      continue;
    }
    orbit_idx.push_back(static_cast<uint32_t>(a));
    orbit_idx.push_back(static_cast<uint32_t>(c));
    orbit_idx.push_back(static_cast<uint32_t>(b));
  }
  if (orbit_idx.size() < 3) {
    return;
  }

  // Geographic elev maps mine clay (~50m) to orbit y~7 while DEM roof is ~0.2.
  // Remap relative relief into a slab just above the DEM roof so purple reads.
  float dem_max_y = -1.0e9f;
  const size_t dem_floats = dem_local_xyz_count_;
  for (size_t i = 1; i < dem_floats && i < local_xyz_.size(); i += 3) {
    dem_max_y = (std::max)(dem_max_y, local_xyz_[i]);
  }
  if (!(dem_max_y > -1.0e8f)) {
    dem_max_y = 0.f;
  }
  const float geo_y0 = mn_y;
  const float geo_span = (std::max)(mx_y - mn_y, 1.0e-4f);
  constexpr float kOrbitClearance = 0.35f;
  constexpr float kOrbitSlab = 0.25f;
  const float target_base = dem_max_y + kOrbitClearance;
  for (size_t i = 0; i < n; ++i) {
    const float t = (orbit_xyz[i * 3 + 1] - geo_y0) / geo_span;
    orbit_xyz[i * 3 + 1] = target_base + t * kOrbitSlab;
  }
  mn_y = target_base;
  mx_y = target_base + kOrbitSlab;
  LOGGING(LOG_INFO,
          "scene3d.present overlay_tin orbit_y=[%.3f,%.3f] geo_y0=%.3f "
          "dem_roof=%.3f verts=%zu idx=%zu",
          mn_y, mx_y, geo_y0, dem_max_y, n, orbit_idx.size());

  // Trim any prior overlay fold (cache-hit DEM rebuild leaves local_* intact).
  if (local_xyz_.size() > dem_local_xyz_count_ ||
      local_idx_.size() > dem_local_idx_count_) {
    local_xyz_.resize(dem_local_xyz_count_);
    local_idx_.resize(dem_local_idx_count_);
  }
  const size_t base_vert = local_xyz_.size() / 3;

  // Inflate XZ so AABB-bridge / sparse TIN still covers the borehole pad.
  constexpr float kPadXz = 0.12f;
  const float ax0 = mn_x - kPadXz;
  const float ax1 = mx_x + kPadXz;
  const float az0 = mn_z - kPadXz;
  const float az1 = mx_z + kPadXz;
  gis::Node* node = terrain_world_.attach_terrain(
      "overlay_tin", ax0, mn_y, az0, ax1, mx_y, az1);
  if (node) {
    // Sparse 5-vert stratum TINs have been silent under FlyCube solid PS even
    // with instance paint; keep mesh for denser overlays, else AABB bridge.
    if (n >= 8) {
      (void)terrain_world_.set_terrain_mesh(node->id, orbit_xyz.data(),
                                            orbit_xyz.size(), orbit_idx.data(),
                                            orbit_idx.size());
    }
  }

  // Fold into the GDI paint buffer; FlyCube draws the World node above.
  local_xyz_.insert(local_xyz_.end(), orbit_xyz.begin(), orbit_xyz.end());
  for (uint32_t vi : orbit_idx) {
    local_idx_.push_back(
        static_cast<unsigned>(base_vert + static_cast<size_t>(vi)));
  }

  // Guarantee purple on the known-good point-cloud path (amber sticks already
  // read). Sparse terrain TIN/AABB paint has been silent under FlyCube solid.
  {
    std::vector<uint8_t> purple(n * 4u);
    for (size_t i = 0; i < n; ++i) {
      purple[i * 4u + 0] = overlay_tin_has_albedo_ ? overlay_tin_albedo_[0]
                                                   : static_cast<uint8_t>(0x8e);
      purple[i * 4u + 1] = overlay_tin_has_albedo_ ? overlay_tin_albedo_[1]
                                                   : static_cast<uint8_t>(0x44);
      purple[i * 4u + 2] = overlay_tin_has_albedo_ ? overlay_tin_albedo_[2]
                                                   : static_cast<uint8_t>(0xad);
      purple[i * 4u + 3] = 255;
    }
    // Drop prior marker cloud.
    for (size_t i = 0; i < terrain_world_.node_count();) {
      const gis::Node* node = terrain_world_.node_at(i);
      if (node && node->kind == gis::NodeKind::kPointCloud &&
          node->name == "overlay_tin_markers") {
        if (!terrain_world_.remove_node(node->id)) {
          ++i;
        }
        continue;
      }
      ++i;
    }
    gis::Node* markers = terrain_world_.attach_pointcloud(
        "overlay_tin_markers", ax0, mn_y, az0, ax1, mx_y, az1);
    if (markers) {
      (void)terrain_world_.set_pointcloud_points(markers->id, orbit_xyz.data(),
                                                 static_cast<int>(n),
                                                 purple.data(),
                                                 static_cast<int>(purple.size()));
    }
  }
  overlay_tin_dirty_ = false;
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
  // then 错佝). Only a lon/lat box inside China may reframe the orbit.
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
    return {};
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
  std::lock_guard<std::mutex> lock(present_mu_);
  if (atmosphere) {
    atmosphere->release_passes();
  }
  mesh_device_ = nullptr;
  dem_gpu_synced_after_ocean_ = false;
  dem_gpu_synced_after_sky_ = false;
  // FlyCube / MapViewport may have shut down (or leaked) the Device already.
  // release() would destroy_pipeline/buffer on a dangling Device* - AV on
  // self-test teardown. Match GpuScene::~GpuScene and drop handles only.
  gpu_scene_.abandon();
}

bool Scene3dGpuPresent::rebuild_local_mesh() {
  // Caller must hold present_mu_ (present / paint).
  BASE_TRACE_EVENT("mesh", "scene3d.mesh");
  // Trim overlay fold so DEM cache keys on DEM-only buffers. Clearing the
  // whole vector forced rebuild_terrain_mesh cache miss every frame (~100ms+).
  if (dem_local_xyz_count_ > 0 && local_xyz_.size() > dem_local_xyz_count_) {
    local_xyz_.resize(dem_local_xyz_count_);
  }
  if (dem_local_idx_count_ > 0 && local_idx_.size() > dem_local_idx_count_) {
    local_idx_.resize(dem_local_idx_count_);
  }

  const size_t xyz_before = local_xyz_.size();
  const size_t idx_before = local_idx_.size();
  const int lod_before = terrain_lod_edge_;
  rebuild_terrain_mesh(&terrain_world_, scene_, world_extent(), distance(),
                       &local_xyz_, &local_idx_, &geo_frame_,
                       &terrain_lod_edge_);
  dem_local_xyz_count_ = local_xyz_.size();
  dem_local_idx_count_ = local_idx_.size();
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
  std::lock_guard<std::mutex> lock(present_mu_);
  remember_view_size(static_cast<int>(width_px), static_cast<int>(height_px));
  base::ElapsedTimer mesh_timer;
  const bool dem_rebuilt = rebuild_local_mesh();
  note_scene3d_phase_mesh(static_cast<int64_t>(
      mesh_timer.elapsed_milliseconds() + 0.5));
  attach_overlay_tin_locked(dem_rebuilt || overlay_tin_dirty_);
  attach_overlay_pointcloud_locked(dem_rebuilt || overlay_pointcloud_dirty_);
  // 3D Tiles product stream (P0-B): select -> LRU ensure when a tileset is attached.
  if (tileset_stream_ && tileset_stream_->active()) {
    tileset_stream_->pump(&terrain_world_, orbit_, 0, 16);
    gpu_scene_.set_tileset_content_cache(&tileset_stream_->cache());
  }
  if (terrain_world_.node_count() == 0) {
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
  }
  base::ElapsedTimer sync_timer;
  gpu_scene_.sync_from(terrain_world_);
  note_scene3d_phase_sync(static_cast<int64_t>(
      sync_timer.elapsed_milliseconds() + 0.5));
  // White tint: draped hypsometric / china_rs keep authored RGB (olive
  // multiply made land read as flat mud under FlyCube textured PS).
  gpu_scene_.set_solid_color(1.f, 1.f, 1.f, 1.f);
  // Overlay TIN: solid albedo as instance paint so FlyCube reads purple/cyan
  // even when 2x2 terrain_rgba upload/sample fails (mine/stormsurge slabs).
  if (overlay_tin_has_albedo_) {
    uint64_t overlay_id = 0;
    for (size_t ni = 0; ni < terrain_world_.node_count(); ++ni) {
      const gis::Node* n = terrain_world_.node_at(ni);
      if (n && n->kind == gis::NodeKind::kTerrain && n->name == "overlay_tin") {
        overlay_id = n->id;
        break;
      }
    }
    for (size_t i = 0; i < gpu_scene_.instance_count(); ++i) {
      const effect::scene::GpuInstance* inst = gpu_scene_.instance_at(i);
      if (!inst || inst->kind != gis::NodeKind::kTerrain) {
        continue;
      }
      const bool by_id = overlay_id != 0 && inst->node_id == overlay_id;
      const bool by_tex =
          inst->terrain_tex_w == 2 && inst->terrain_tex_h == 2 &&
          inst->terrain_rgba.size() >= 16 &&
          inst->terrain_rgba[0] == overlay_tin_albedo_[0] &&
          inst->terrain_rgba[1] == overlay_tin_albedo_[1] &&
          inst->terrain_rgba[2] == overlay_tin_albedo_[2];
      if (!by_id && !by_tex) {
        continue;
      }
      if (inst->has_paint) {
        break;
      }
      gis::style::ResolvedPaint fill;
      fill.type = gis::style::LayerType::kFill;
      // Opaque ARGB - avoid double-multiplying alpha via fill_opacity.
      fill.fill_color = 0xFF000000u |
                         (static_cast<uint32_t>(overlay_tin_albedo_[0]) << 16) |
                         (static_cast<uint32_t>(overlay_tin_albedo_[1]) << 8) |
                         static_cast<uint32_t>(overlay_tin_albedo_[2]);
      fill.fill_opacity = 1.f;
      (void)gpu_scene_.set_instance_paint(i, fill);
      LOGGING(LOG_INFO,
              "scene3d.present overlay_tin paint id=%llu inst=%zu by_id=%d "
              "by_tex=%d rgba=%u,%u,%u,%u",
              static_cast<unsigned long long>(overlay_id), i, by_id ? 1 : 0,
              by_tex ? 1 : 0, overlay_tin_albedo_[0], overlay_tin_albedo_[1],
              overlay_tin_albedo_[2], overlay_tin_albedo_[3]);
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
  } else {
    gpu_scene_.clear_background_paint();
  }
  // Drive DEM Lambert from atmosphere sun (azimuth / elevation scrub with time).
  {
    // Start from program defaults so a missing Environment never zero-lights
    // untextured meshes into pure-black slabs.
    render::programs::Light light;
    if (const gis::atmosphere::Environment* env_light =
            atmosphere.environment()) {
      const float az = env_light->params().sun_azimuth_rad;
      const float el = env_light->params().sun_elevation_rad;
      const float cos_el = std::cos(el);
      light.dir_x = std::cos(az) * cos_el;
      light.dir_y = -std::sin(el);
      light.dir_z = std::sin(az) * cos_el;
      if (look_preset_ == Scene3dLookPreset::kLegacyStereo) {
        // Leftover stereo: harder key light on vertical DEM faces.
        light.ambient = 0.28f;
        light.intensity = 1.85f;
      } else {
        // Soft fill floor so hypsometric greens stay landish-gate visible
        // under sky (lit textured * low ambient previously read as near-black
        // (21,0,0) mainland silhouettes on atmosphere.full BMPs).
        light.ambient = 0.72f;
        light.intensity = 1.15f;
      }
    }
    gpu_scene_.set_light(light);
  }
  // Verbose DEM / AABB dump once (or when SMT_SCENE3D_PRESENT_LOG=1). Scanning
  // every vertex each frame was a measurable FPS tax in Debug builds.
  {
    static bool logged_once = false;
    const bool force_log = []() {
      if (const char* e = std::getenv("SMT_SCENE3D_PRESENT_LOG")) {
        return e[0] == '1' && e[1] == '\0';
      }
      return false;
    }();
    if (force_log || !logged_once) {
      logged_once = true;
      size_t tex_nodes = 0;
      for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
        const gis::Node* n = terrain_world_.node_at(i);
        if (n && !n->terrain_rgba.empty() && n->terrain_tex_w > 0 &&
            n->terrain_tex_h > 0) {
          ++tex_nodes;
        }
      }
      LOGGING(LOG_INFO,
              "scene3d.present dem nodes=%zu textured=%zu size=%ux%u "
              "yaw=%.2f pitch=%.2f dist=%.2f",
              terrain_world_.node_count(), tex_nodes, width_px, height_px,
              yaw(), pitch(), distance());
      // One-shot albedo fingerprint: atmosphere.full black mainland was
      // (21,0,0) while CPU bake stayed green � distinguish upload vs shade.
      for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
        const gis::Node* n = terrain_world_.node_at(i);
        if (!n || n->terrain_rgba.empty() || n->terrain_tex_w == 0) {
          continue;
        }
        uint64_t sr = 0;
        uint64_t sg = 0;
        uint64_t sb = 0;
        const size_t npx =
            static_cast<size_t>(n->terrain_tex_w) *
            static_cast<size_t>(n->terrain_tex_h);
        const size_t nbytes = (std::min)(n->terrain_rgba.size(), npx * 4u);
        size_t count = 0;
        for (size_t p = 0; p + 3 < nbytes; p += 4) {
          sr += n->terrain_rgba[p + 0];
          sg += n->terrain_rgba[p + 1];
          sb += n->terrain_rgba[p + 2];
          ++count;
        }
        if (count > 0) {
          LOGGING(LOG_INFO,
                  "scene3d.present dem albedo mean_rgb=%u,%u,%u tex=%ux%u",
                  static_cast<unsigned>(sr / count),
                  static_cast<unsigned>(sg / count),
                  static_cast<unsigned>(sb / count), n->terrain_tex_w,
                  n->terrain_tex_h);
        }
        break;
      }
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
  render_engine_name = render::rhi::backend_display_name(device->backend());
  note_present_frame();

  const gis::atmosphere::Environment* env = atmosphere.environment();
  const bool ocean_on = env && env->ocean_enabled();
  const bool cloud_on = env && env->cloud_enabled();
  const bool sky_on = [&]() {
    if (!env || !env->sky_enabled()) {
      return false;
    }
    if (const char* e = std::getenv("SMT_ATMOSPHERE_SKIP_SKY")) {
      if (e[0] == '1' && e[1] == '\0') {
        return false;
      }
    }
    return true;
  }();
  const bool fog_on = env && env->fog_enabled();
  // Record order (ocean SRV0 must not precede textured DEM):
  //   sky (+ depth clear) -> opaque DEM -> ocean -> cloud/fog
  // Ocean stays off in AtmosphereFrame pre; drawn after opaque below.
  atmosphere.frame().set_ocean_enabled(false);
  atmosphere.frame().set_cloud_enabled(false);
  atmosphere.frame().set_sky_enabled(sky_on);
  atmosphere.frame().set_fog_enabled(false);
  if (!atmosphere.prepare_for_present()) {
    LOGGING(LOG_ERROR, "scene3d.present fail: atmosphere.prepare_for_present");
    return false;
  }
  // Optional isolate: SMT_ATMOSPHERE_SKIP_OCEAN=1 keeps sky/DEM without ocean
  // (debug atmosphere.full near-black China). Skip prepare_gpu too � height
  // texture alloc still recycles FlyCube SRVs and blacks DEM albedo.
  const bool skip_ocean = []() {
    if (const char* e = std::getenv("SMT_ATMOSPHERE_SKIP_OCEAN")) {
      return e[0] == '1' && e[1] == '\0';
    }
    return false;
  }();
  // Allocate ocean height BEFORE DEM albedo upload. Creating height after the
  // DEM draw is recorded left mesh.texture pointing at recycled height bytes
  // (atmosphere.full near-black China).
  base::ElapsedTimer ocean_prep_timer;
  // Cold path only: allocate ocean height before DEM remesh so FlyCube does
  // not recycle hypsometric albedo as the height map. Warm frames (already
  // synced after ocean) skip prepare_gpu � OceanPass::record does one
  // Gerstner/upload instead of prepare_gpu + record double work.
  const bool need_ocean_height_before_dem =
      ocean_on && !skip_ocean && !dem_gpu_synced_after_ocean_;
  if (need_ocean_height_before_dem &&
      !atmosphere.ocean_pass().prepare_gpu(device)) {
    LOGGING(LOG_ERROR, "scene3d.present fail: ocean.prepare_gpu");
    return false;
  }
  note_scene3d_phase_ocean_prep(static_cast<int64_t>(
      ocean_prep_timer.elapsed_milliseconds() + 0.5));
  // Re-sync DEM after ocean height allocation so albedo textures are created
  // after height (avoids FlyCube recycling hypsometric SRVs as height maps).
  if (need_ocean_height_before_dem) {
    gpu_scene_.sync_from(terrain_world_);
  }
  // First ocean-height / sky-depth alloc can recycle FlyCube heap that still
  // backs DEM albedo. Force one remesh after those resources exist; warm
  // frames keep StaticReuse (height upload is in-place).
  if (need_ocean_height_before_dem) {
    gpu_scene_.mark_meshes_dirty();
  }
  if (sky_on && !dem_gpu_synced_after_sky_) {
    gpu_scene_.mark_meshes_dirty();
  }
  // FlyCube + AtmosphereFrame: lit textured DEM often samples near-black
  // (21,0,0) even with high ambient. Prefer solid fill tinted by the DEM
  // albedo mean so landish gates stay green; bake a mild height ramp into
  // the mean so the mainland is not a single flat olive slab.
  gpu_scene_.set_solid_color(1.f, 1.f, 1.f, 1.f);
  if (sky_on) {
    float ar = 0.28f;
    float ag = 0.52f;
    float ab = 0.22f;
    float amin = 1.f;
    float amax = 0.f;
    for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
      const gis::Node* n = terrain_world_.node_at(i);
      if (!n || n->terrain_rgba.size() < 4) {
        continue;
      }
      uint64_t sr = 0;
      uint64_t sg = 0;
      uint64_t sb = 0;
      size_t count = 0;
      for (size_t p = 0; p + 3 < n->terrain_rgba.size(); p += 4) {
        const float lum = (n->terrain_rgba[p + 0] * 0.3f +
                           n->terrain_rgba[p + 1] * 0.59f +
                           n->terrain_rgba[p + 2] * 0.11f) /
                          255.f;
        amin = (std::min)(amin, lum);
        amax = (std::max)(amax, lum);
        sr += n->terrain_rgba[p + 0];
        sg += n->terrain_rgba[p + 1];
        sb += n->terrain_rgba[p + 2];
        ++count;
      }
      if (count > 0) {
        ar = static_cast<float>(sr / count) / 255.f;
        ag = static_cast<float>(sg / count) / 255.f;
        ab = static_cast<float>(sb / count) / 255.f;
        // Lift chroma toward hypsometric greens/browns when albedo span is flat.
        if (amax - amin < 0.08f) {
          ar = 0.34f;
          ag = 0.58f;
          ab = 0.24f;
        } else {
          ar = (std::min)(1.f, ar * 1.15f);
          ag = (std::min)(1.f, ag * 1.20f);
          ab = (std::min)(1.f, ab * 1.05f);
        }
      }
      break;
    }
    gpu_scene_.set_solid_color(ar, ag, ab, 1.f);
    _putenv_s("SMT_SCENE3D_SOLID_TERRAIN", "1");
  } else {
    _putenv_s("SMT_SCENE3D_SOLID_TERRAIN", "0");
  }

  const int cloud_quality = (env && cloud_on) ? env->params().quality : 1;
  effect::scene::OpaqueEffect opaque(&gpu_scene_);
  // This HWND is the swapchain. Parent chrome is painted on the widget, not
  // here. A fullscreen shell quad replaces the terrain a frame later
  // (correct flash, then a shifted / flat cover). Do not composite it.
  (void)shell;
  (void)shell_generation;
  shell_overlay_.clear();

  render::rhi::CommandList* list = device->create_command_list();
  if (!list) {
    LOGGING(LOG_ERROR, "scene3d.present fail: create_command_list");
    return false;
  }
  render::graph::RecordContext ctx;
  ctx.device = device;
  ctx.list = list;
  ctx.width = width_px;
  ctx.height = height_px;
  ctx.camera = view_camera;
  ctx.color_op = render::rhi::ColorLoadOp::kClear;
  ctx.shared_depth = false;

  effect::atmosphere::AtmosphereEffects atmosphere_effects(&atmosphere.frame(),
                                                           cloud_quality);
  base::ElapsedTimer record_timer;
  bool ok = atmosphere_effects.pre_effect()->record(ctx);
  if (atmosphere_effects.pre_effect()->clears_color()) {
    ctx.color_op = render::rhi::ColorLoadOp::kLoad;
  }
  if (atmosphere_effects.pre_effect()->uses_shared_depth()) {
    ctx.shared_depth = true;
  }
  // Sky/depth first open can recycle DEM SRVs uploaded before pre. Remesh
  // once after sky resources exist; later frames skip.
  if (sky_on && !dem_gpu_synced_after_sky_) {
    gpu_scene_.mark_meshes_dirty();
  } else if (need_ocean_height_before_dem) {
    gpu_scene_.mark_meshes_dirty();
  }
  base::ElapsedTimer rebuild_timer;
  const bool need_rebuild =
      gpu_scene_.needs_mesh_upload(device, width_px, height_px);
  if (!gpu_scene_.ensure_meshes(device, width_px, height_px)) {
    device->destroy_command_list(list);
    LOGGING(LOG_ERROR, "scene3d.present fail: ensure_meshes");
    return false;
  }
  note_scene3d_phase_rebuild(
      static_cast<int64_t>(rebuild_timer.elapsed_milliseconds() + 0.5),
      need_rebuild ? 1 : 0);
  ok = opaque.record(ctx) && ok;

  if (ocean_on && !skip_ocean) {
    render::rhi::RenderPassDesc ocean_pass;
    ocean_pass.width = width_px;
    ocean_pass.height = height_px;
    ocean_pass.load_op = render::rhi::ColorLoadOp::kLoad;
    ocean_pass.enable_depth = true;
    ocean_pass.depth_load_op = render::rhi::DepthLoadOp::kLoad;
    list->begin_render_pass(ocean_pass);
    if (view_camera) {
      list->bind_camera(*view_camera);
    }
    ok = atmosphere.ocean_pass().record(device, list, width_px, height_px,
                                        view_camera) &&
         ok;
    list->end_render_pass();
  }
  if (cloud_on || fog_on) {
    const bool skip_post = []() {
      if (const char* e = std::getenv("SMT_ATMOSPHERE_SKIP_POST")) {
        return e[0] == '1' && e[1] == '\0';
      }
      return false;
    }();
    if (!skip_post) {
    atmosphere.frame().set_cloud_enabled(cloud_on);
    atmosphere.frame().set_fog_enabled(fog_on);
    ok = atmosphere.frame().record_post_opaque(device, list, width_px, height_px,
                                               view_camera, cloud_quality) &&
         ok;
    }
  }
  list->close();
  if (!ok || !device->execute(list)) {
    device->destroy_command_list(list);
    LOGGING(LOG_ERROR,
            "scene3d.present fail: execute size=%ux%u nodes=%zu backend=%s",
            width_px, height_px, terrain_world_.node_count(),
            render_engine_name);
    return false;
  }
  device->destroy_command_list(list);
  note_scene3d_phase_record(static_cast<int64_t>(
      record_timer.elapsed_milliseconds() + 0.5));
  base::ElapsedTimer present_timer;
  device->present();
  note_scene3d_phase_present(static_cast<int64_t>(
      present_timer.elapsed_milliseconds() + 0.5));
  if (ok) {
    if (ocean_on && !skip_ocean) {
      dem_gpu_synced_after_ocean_ = true;
    }
    if (sky_on) {
      dem_gpu_synced_after_sky_ = true;
    }
  }
  return true;
}

}  // namespace content
