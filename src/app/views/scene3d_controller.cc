// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/scene3d_controller.h"

#include "app/views/map_host_extent.h"
#include "app/views/map_scene.h"
#include "content/public/map_contents.h"
#include "gis/atmosphere/atmosphere_params.h"
#include "gis/atmosphere/cloud_system.h"
#include "gis/atmosphere/field_channel.h"
#include "gis/atmosphere/field_ingest.h"
#include "gis/atmosphere/ocean_system.h"
#include "gis/world/dem_frame.h"
#include "gis/world/dem_raster.h"
#include "gis/world/land_mask.h"
#include "gis/world/scene.h"
#include "render/atmosphere/ocean_pass.h"
#include "render/rhi/rhi.h"
#include "render/scene/scene.h"
#include "tool/camera_nav.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace app {
namespace {

constexpr float kFovY = 0.785398f;  // ~45 deg

void normalize_mesh(std::vector<float>* xyz) {
  if (!xyz || xyz->size() < 3) {
    return;
  }
  float minx = (*xyz)[0];
  float maxx = minx;
  float miny = (*xyz)[1];
  float maxy = miny;
  float minz = (*xyz)[2];
  float maxz = minz;
  for (size_t i = 0; i + 2 < xyz->size(); i += 3) {
    minx = (std::min)(minx, (*xyz)[i]);
    maxx = (std::max)(maxx, (*xyz)[i]);
    miny = (std::min)(miny, (*xyz)[i + 1]);
    maxy = (std::max)(maxy, (*xyz)[i + 1]);
    minz = (std::min)(minz, (*xyz)[i + 2]);
    maxz = (std::max)(maxz, (*xyz)[i + 2]);
  }
  const float cx = 0.5f * (minx + maxx);
  const float cy = 0.5f * (miny + maxy);
  const float cz = 0.5f * (minz + maxz);
  const float span = (std::max)(maxx - minx, (std::max)(maxz - minz, 1.f));
  const float s = 3.2f / span;
  // Extra elev scale after XY normalize so pitch reveals mountains (option b).
  constexpr float kElevBoost = 1.6f;
  for (size_t i = 0; i + 2 < xyz->size(); i += 3) {
    (*xyz)[i] = ((*xyz)[i] - cx) * s;
    (*xyz)[i + 1] = ((*xyz)[i + 1] - cy) * s * kElevBoost;
    (*xyz)[i + 2] = ((*xyz)[i + 2] - cz) * s;
  }
}

}  // namespace

Scene3dController::Scene3dController() {
  extent_ = kChinaLonLatExtent;
}

Scene3dController::~Scene3dController() {
  release_atmosphere_passes();
  release_mesh();
}

void Scene3dController::bind_map(const MapScene* scene) {
  scene_ = scene;
  local_xyz_.clear();
  local_idx_.clear();
  abandon_mesh();
  pull_extent_from_contents();
}

void Scene3dController::bind_contents(content::MapContents* session,
                                      uint32_t view_id) {
  contents_ = session;
  view_id_ = view_id;
  pull_extent_from_contents();
}

void Scene3dController::abandon_mesh() {
  release_atmosphere_passes();
  mesh_device_ = nullptr;
  gpu_scene_.release();
}

void Scene3dController::release_atmosphere_passes() {
  ocean_pass_.release();
  cloud_pass_.release();
}

gis::atmosphere::Environment& Scene3dController::ensure_atmosphere() {
  if (!atmosphere_) {
    atmosphere_ = std::make_unique<gis::atmosphere::Environment>();
  }
  return *atmosphere_;
}

void Scene3dController::set_ocean_enabled(bool on) {
  ensure_atmosphere().set_ocean_enabled(on);
}

void Scene3dController::set_cloud_enabled(bool on) {
  ensure_atmosphere().set_cloud_enabled(on);
}

void Scene3dController::set_wind_overlay_enabled(bool on) {
  wind_overlay_enabled_ = on;
  if (on) {
    // Need WindU/V samples for arrows; seed procedural if store empty.
    gis::atmosphere::Environment& env = ensure_atmosphere();
    if (env.field_store().layer_count() == 0) {
      seed_atmosphere_procedural();
    }
  }
}

void Scene3dController::set_time_sec(double t) {
  gis::atmosphere::Environment& env = ensure_atmosphere();
  env.scrub_time_sec(t);
  // Prefer clamp against External wave/cover/wind if a timed range exists.
  static const gis::atmosphere::FieldChannel kClampOrder[] = {
      gis::atmosphere::FieldChannel::kWaveHs,
      gis::atmosphere::FieldChannel::kCloudCover,
      gis::atmosphere::FieldChannel::kWindU,
      gis::atmosphere::FieldChannel::kWindV,
      gis::atmosphere::FieldChannel::kWaveDir,
      gis::atmosphere::FieldChannel::kCloudBase,
      gis::atmosphere::FieldChannel::kCloudTop,
      gis::atmosphere::FieldChannel::kSeaMask,
  };
  for (gis::atmosphere::FieldChannel ch : kClampOrder) {
    if (env.clamp_time_to_field(ch)) {
      break;
    }
  }
}

double Scene3dController::time_sec() const {
  return atmosphere_ ? atmosphere_->time_sec() : 0.0;
}

namespace {

gis::atmosphere::FieldChannel parse_field_channel(std::string_view name,
                                                  bool* ok) {
  *ok = true;
  if (name == "wind_u" || name == "u") {
    return gis::atmosphere::FieldChannel::kWindU;
  }
  if (name == "wind_v" || name == "v") {
    return gis::atmosphere::FieldChannel::kWindV;
  }
  if (name == "wave_hs" || name == "hs") {
    return gis::atmosphere::FieldChannel::kWaveHs;
  }
  if (name == "wave_dir" || name == "dir") {
    return gis::atmosphere::FieldChannel::kWaveDir;
  }
  if (name == "cloud_cover" || name == "cover" || name == "cloud") {
    return gis::atmosphere::FieldChannel::kCloudCover;
  }
  if (name == "cloud_base" || name == "base") {
    return gis::atmosphere::FieldChannel::kCloudBase;
  }
  if (name == "cloud_top" || name == "top") {
    return gis::atmosphere::FieldChannel::kCloudTop;
  }
  if (name == "sea_mask" || name == "sea") {
    return gis::atmosphere::FieldChannel::kSeaMask;
  }
  *ok = false;
  return gis::atmosphere::FieldChannel::kCloudCover;
}

// Trim ASCII whitespace from both ends of |s|.
std::string_view trim_ascii(std::string_view s) {
  while (!s.empty() &&
         (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' ||
          s.front() == '\n')) {
    s.remove_prefix(1);
  }
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ||
          s.back() == '\n')) {
    s.remove_suffix(1);
  }
  return s;
}

struct FieldSeriesEntry {
  std::string path;
  double time_sec = 0.0;
};

}  // namespace

bool Scene3dController::load_atmosphere_fields(std::string_view spec) {
  // Parse path[:channel[:time]][,...] then batch by channel into
  // Environment::load_external_series (ingest_gdal_field_series).
  spec = trim_ascii(spec);
  if (spec.empty()) {
    return false;
  }
  gis::atmosphere::Environment& env = ensure_atmosphere();

  // Preserve input order of first appearance per channel.
  std::vector<gis::atmosphere::FieldChannel> channel_order;
  std::vector<std::vector<FieldSeriesEntry>> series_by_channel(
      static_cast<std::size_t>(gis::atmosphere::FieldChannel::kCount));

  int default_time_index = 0;
  size_t begin = 0;
  while (begin <= spec.size()) {
    size_t comma = spec.find(',', begin);
    if (comma == std::string_view::npos) {
      comma = spec.size();
    }
    std::string_view entry = trim_ascii(spec.substr(begin, comma - begin));
    begin = comma + 1;
    if (entry.empty()) {
      if (begin > spec.size()) {
        break;
      }
      continue;
    }

    std::string_view path = entry;
    std::string_view channel_name;
    double time_sec = static_cast<double>(default_time_index);
    const size_t c1 = entry.find(':');
    if (c1 != std::string_view::npos) {
      path = trim_ascii(entry.substr(0, c1));
      std::string_view rest = trim_ascii(entry.substr(c1 + 1));
      const size_t c2 = rest.find(':');
      if (c2 == std::string_view::npos) {
        channel_name = rest;
      } else {
        channel_name = trim_ascii(rest.substr(0, c2));
        std::string time_s(trim_ascii(rest.substr(c2 + 1)));
        if (!time_s.empty()) {
          time_sec = std::atof(time_s.c_str());
        }
      }
    }

    bool channel_ok = true;
    gis::atmosphere::FieldChannel channel =
        gis::atmosphere::FieldChannel::kCloudCover;
    if (!channel_name.empty()) {
      channel = parse_field_channel(channel_name, &channel_ok);
      if (!channel_ok) {
        std::fprintf(stderr,
                     "atmosphere-fields: unknown channel '%.*s' for %.*s\n",
                     static_cast<int>(channel_name.size()), channel_name.data(),
                     static_cast<int>(path.size()), path.data());
        if (begin > spec.size()) {
          break;
        }
        continue;
      }
    }

    if (path.empty()) {
      if (begin > spec.size()) {
        break;
      }
      continue;
    }

    const std::size_t ch_i = static_cast<std::size_t>(channel);
    if (series_by_channel[ch_i].empty()) {
      channel_order.push_back(channel);
    }
    FieldSeriesEntry slice;
    slice.path = std::string(path);
    slice.time_sec = time_sec;
    series_by_channel[ch_i].push_back(std::move(slice));
    ++default_time_index;
    if (begin > spec.size()) {
      break;
    }
  }

  if (channel_order.empty()) {
    return false;
  }

  gis::atmosphere::FieldIngestOptions opts;
  opts.kind = gis::atmosphere::FieldSourceKind::kExternal;
  opts.priority = 20;

  bool any = false;
  gis::atmosphere::FieldChannel first_ok =
      gis::atmosphere::FieldChannel::kCloudCover;
  for (gis::atmosphere::FieldChannel channel : channel_order) {
    const auto& slices =
        series_by_channel[static_cast<std::size_t>(channel)];
    std::vector<const char*> paths;
    std::vector<double> times;
    paths.reserve(slices.size());
    times.reserve(slices.size());
    for (const FieldSeriesEntry& s : slices) {
      paths.push_back(s.path.c_str());
      times.push_back(s.time_sec);
    }
    if (!env.load_external_series(channel, paths.data(), times.data(),
                                  paths.size(), opts)) {
      std::fprintf(stderr,
                   "atmosphere-fields: load_external_series failed for channel "
                   "%d (%zu files)\n",
                   static_cast<int>(channel), paths.size());
      continue;
    }
    if (!any) {
      first_ok = channel;
      any = true;
    }
  }

  if (any) {
    env.clamp_time_to_field(first_ok);
    env.sync_systems_from_params();
  }
  return any;
}

gis::atmosphere::FieldGrid Scene3dController::atmosphere_field_grid() const {
  const content::Extent2 e = world_extent();
  gis::atmosphere::FieldGrid grid;
  grid.min_lon = e.xmin;
  grid.min_lat = e.ymin;
  grid.max_lon = e.xmax;
  grid.max_lat = e.ymax;
  grid.cols = 32;
  grid.rows = 32;
  return grid;
}

void Scene3dController::seed_atmosphere_procedural() {
  gis::atmosphere::Environment& env = ensure_atmosphere();
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  env.seed_procedural_baseline(atmosphere_field_grid(),
                               rings.empty() ? nullptr : &rings);
  env.sync_systems_from_params();
}

void Scene3dController::enable_atmosphere_demo() {
  gis::atmosphere::Environment& env = ensure_atmosphere();
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  env.enable_demo(atmosphere_field_grid(),
                  rings.empty() ? nullptr : &rings);
}

void Scene3dController::release_mesh() {
  abandon_mesh();
}

void Scene3dController::reset() {
  yaw_ = kScene3dDefaultYaw;
  pitch_ = 0.4f;
  distance_ = 3.2f;
  has_last_ = false;
}

void Scene3dController::remember_view_size(int width_px, int height_px) const {
  if (width_px > 0) {
    last_w_ = width_px;
  }
  if (height_px > 0) {
    last_h_ = height_px;
  }
}

void Scene3dController::apply_wheel_at(int view_x, int view_y, int32_t wheel,
                                       int view_w, int view_h) {
  remember_view_size(view_w, view_h);
  const float w = view_w > 0 ? static_cast<float>(view_w) : 1.f;
  const float h = view_h > 0 ? static_cast<float>(view_h) : 1.f;
  const float nx = 2.f * static_cast<float>(view_x) / w - 1.f;
  const float ny = 1.f - 2.f * static_cast<float>(view_y) / h;
  const double factor = tool::wheel_zoom_factor(wheel);
  const float pull = static_cast<float>(1.0 - 1.0 / factor);
  yaw_ += nx * 0.12f * pull;
  pitch_ += ny * 0.08f * pull;
  pitch_ = std::clamp(pitch_, tool::kOrbitPitchMin, tool::kOrbitPitchMax);
  distance_ = tool::dolly_distance(distance_, wheel, 1.2f, 12.f);
}

void Scene3dController::apply_pan(int dx_px, int dy_px) {
  // Horizontal pan orbits yaw. Vertical pan dollies — do not pitch toward
  // edge-on (that collapsed the DEM into a thin green strip).
  yaw_ += static_cast<float>(dx_px) * 0.002f;
  if (dy_px != 0) {
    const int32_t wheel = dy_px > 0 ? -120 : 120;
    distance_ = tool::dolly_distance(distance_, wheel, 1.2f, 12.f);
  }
}

void Scene3dController::apply_pinch(int view_x, int view_y, double scale,
                                    int view_w, int view_h) {
  const int32_t wheel = tool::scale_to_wheel_delta(scale);
  if (wheel == 0) {
    return;
  }
  apply_wheel_at(view_x, view_y, wheel, view_w, view_h);
}

content::Extent2 Scene3dController::world_extent() const {
  return china_or(extent_);
}

void Scene3dController::apply_world_extent(const content::Extent2& e) {
  if (!extent_nonempty(e)) {
    return;
  }
  extent_ = e;
}

void Scene3dController::push_extent_to_contents() {
  if (!contents_ || view_id_ == 0) {
    return;
  }
  contents_->SetExtent(view_id_, world_extent());
}

void Scene3dController::pull_extent_from_contents() {
  if (contents_ && view_id_ != 0) {
    const content::Extent2 e = contents_->Extent(view_id_);
    if (extent_nonempty(e)) {
      extent_ = e;
      return;
    }
  }
  if (scene_) {
    const content::Extent2 e = scene_->world_extent();
    if (extent_nonempty(e)) {
      extent_ = e;
      return;
    }
  }
  if (!extent_nonempty(extent_)) {
    extent_ = kChinaLonLatExtent;
  }
}

void Scene3dController::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kWheel) {
    if (!draft.points.empty() && last_w_ > 0 && last_h_ > 0) {
      apply_wheel_at(draft.points.front().x_px, draft.points.front().y_px,
                     draft.wheel, last_w_, last_h_);
    } else {
      distance_ = tool::dolly_distance(distance_, draft.wheel, 1.2f, 12.f);
    }
    return;
  }

  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    const int dx = draft.points[1].x_px - draft.points[0].x_px;
    const int dy = draft.points[1].y_px - draft.points[0].y_px;
    const bool orbit = (draft.flags & (MK_RBUTTON | MK_MBUTTON)) != 0;
    if (orbit) {
      tool::orbit_from_drag(&yaw_, &pitch_, dx, dy, 0.01f);
    } else {
      apply_pan(dx, dy);
    }
    last_x_ = draft.points[1].x_px;
    last_y_ = draft.points[1].y_px;
    has_last_ = true;
    return;
  }

  if (draft.points.empty()) {
    return;
  }

  if (draft.kind == tool::DraftKind::kPoint) {
    const int x = draft.points.front().x_px;
    const int y = draft.points.front().y_px;
    if (has_last_) {
      tool::orbit_from_drag(&yaw_, &pitch_, x - last_x_, y - last_y_, 0.01f);
    }
    last_x_ = x;
    last_y_ = y;
    has_last_ = true;
  }
}

render::rhi::CameraMatrices Scene3dController::camera_matrices(
    float aspect) const {
  return render::rhi::make_orbit_camera(yaw_, pitch_, distance_, kFovY, aspect,
                                        0.1f, 100.f);
}

render::rhi::CameraMatrices Scene3dController::camera_matrices_ortho(
    float width_px, float height_px) const {
  (void)width_px;
  (void)height_px;
  const content::Extent2 e = world_extent();
  return render::rhi::make_ortho_camera(
      static_cast<float>(e.xmin), static_cast<float>(e.xmax),
      static_cast<float>(e.ymin), static_cast<float>(e.ymax), -1.f, 1.f);
}

void Scene3dController::rebuild_local_mesh() {
  if (!local_xyz_.empty() && !local_idx_.empty()) {
    return;
  }
  // Clear prior terrain nodes, then seed via GIS DEM API (no leftover DEM).
  while (terrain_world_.node_count() > 0) {
    const gis::Node* n = terrain_world_.node_at(0);
    if (!n || !terrain_world_.remove_node(n->id)) {
      break;
    }
  }
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  gis::Node* node = gis::seed_china_dem_into_world(
      &terrain_world_, rings.empty() ? nullptr : rings.data(), rings.size(),
      "views_dem", 96);
  if (node && node->has_terrain_mesh()) {
    local_xyz_ = node->terrain_positions;
    local_idx_.assign(node->terrain_indices.begin(),
                      node->terrain_indices.end());
    normalize_mesh(&local_xyz_);
    // Keep World mesh in orbit-normalized space so GpuScene::record matches
    // the previous unit-cube orbit framing.
    std::vector<uint32_t> idx(local_idx_.begin(), local_idx_.end());
    terrain_world_.set_terrain_mesh(node->id, local_xyz_.data(),
                                    local_xyz_.size(), idx.data(), idx.size());
  }
}

bool Scene3dController::record_atmosphere_ocean(
    render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
    uint32_t height, const render::rhi::CameraMatrices& camera) {
  if (!atmosphere_ || !atmosphere_->ocean_enabled()) {
    return true;
  }
  atmosphere_->sync_systems_from_params();

  const content::Extent2 e = world_extent();
  gis::atmosphere::FieldGrid extent;
  extent.min_lon = e.xmin;
  extent.min_lat = e.ymin;
  extent.max_lon = e.xmax;
  extent.max_lat = e.ymax;
  extent.cols = 1;
  extent.rows = 1;

  const gis::atmosphere::OceanTileParams tile =
      atmosphere_->ocean_system().sample_tile(
          atmosphere_->field_store(), extent, atmosphere_->time_sec());

  render::atmosphere::OceanDrawParams draw;
  draw.significant_wave_height = tile.spectrum.significant_wave_height;
  draw.mean_direction_rad = tile.spectrum.mean_direction_rad;
  draw.wind_speed = tile.spectrum.wind_speed;
  draw.wind_direction_rad = tile.spectrum.wind_direction_rad;
  draw.fft_size = tile.spectrum.fft_size;
  draw.use_gerstner_fallback = tile.spectrum.use_gerstner_fallback;
  draw.use_jonswap = tile.spectrum.use_jonswap;
  draw.chop = tile.spectrum.chop;
  draw.jonswap_gamma = tile.spectrum.jonswap_gamma;
  // Orbit-normalized world uses ~unit extents; keep the patch in frame.
  draw.patch_half_extent = 2.4f;
  draw.mesh_resolution = 17;
  ocean_pass_.set_params(draw);
  ocean_pass_.set_time_sec(atmosphere_->time_sec());

  constexpr int kMask = 16;
  std::vector<float> mask(
      static_cast<std::size_t>(kMask) * static_cast<std::size_t>(kMask), 1.f);
  extent.cols = kMask;
  extent.rows = kMask;
  if (!atmosphere_->ocean_system().fill_sea_mask_grid(
          atmosphere_->field_store(), extent, kMask, kMask,
          atmosphere_->time_sec(), mask.data(), mask.size())) {
    // No mask layer yet — treat whole patch as sea.
    std::fill(mask.begin(), mask.end(), 1.f);
  }
  ocean_pass_.set_sea_mask_cpu(kMask, kMask, mask.data(), mask.size());

  render::rhi::RenderPassDesc pass;
  pass.width = width;
  pass.height = height;
  pass.clear_r = 0.05f;
  pass.clear_g = 0.12f;
  pass.clear_b = 0.22f;
  pass.clear_a = 1.f;
  pass.load_op = render::rhi::ColorLoadOp::kClear;
  pass.enable_depth = true;
  pass.depth_load_op = render::rhi::DepthLoadOp::kClear;
  pass.depth_clear = 1.f;
  list->begin_render_pass(pass);
  const bool ok =
      ocean_pass_.record(device, list, width, height, &camera);
  list->end_render_pass();
  return ok;
}

bool Scene3dController::record_atmosphere_clouds(
    render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
    uint32_t height, const render::rhi::CameraMatrices& camera) {
  if (!atmosphere_ || !atmosphere_->cloud_enabled()) {
    return true;
  }
  const content::Extent2 e = world_extent();
  const double clon = 0.5 * (e.xmin + e.xmax);
  const double clat = 0.5 * (e.ymin + e.ymax);
  const gis::atmosphere::CloudSample sample =
      atmosphere_->cloud_system().sample_at(
          atmosphere_->field_store(), clon, clat, atmosphere_->time_sec());

  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  cloud_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                             p.sun_elevation_rad);
  cloud_pass_.set_cloud_slab(sample.base_m, sample.top_m);
  cloud_pass_.set_cover_modulation(sample.cover);
  return cloud_pass_.record(device, list, width, height, &camera, p.quality);
}

bool Scene3dController::present_gpu(render::rhi::Device* device,
                                    uint32_t width_px, uint32_t height_px) {
  if (!device || width_px == 0 || height_px == 0) {
    return false;
  }
  remember_view_size(static_cast<int>(width_px), static_cast<int>(height_px));
  rebuild_local_mesh();
  if (terrain_world_.node_count() == 0) {
    return false;
  }
  if (mesh_device_ != device) {
    release_atmosphere_passes();
    gpu_scene_.release();
    mesh_device_ = device;
  }
  gpu_scene_.sync_from(terrain_world_);
  // Mid hypsometric (yellow-green highland foothills) — not flat olive FlyCube.
  gpu_scene_.set_solid_color(0.78f, 0.72f, 0.42f, 1.f);
  const float aspect = static_cast<float>(width_px) /
                       static_cast<float>(height_px > 0 ? height_px : 1);
  const render::rhi::CameraMatrices cam = camera_matrices(aspect);
  // Bind orbit camera for FlyCube; Null/tests still get correct draws without
  // CPU frustum cull (avoids StubCommandList ABI issues seen under lit+cull).
  if (device->backend() != render::rhi::Backend::kNull) {
    gpu_scene_.set_view_camera(cam);
  } else {
    gpu_scene_.clear_view_camera();
  }

  render::rhi::CommandList* list = device->create_command_list();
  if (!list) {
    return false;
  }

  // Draw order: ocean (clear color+depth) → land (load) → clouds (load, alpha).
  // FlyCube executes each begin/end_render_pass as a real sequential GPU pass.
  const bool ocean_on = atmosphere_ && atmosphere_->ocean_enabled();
  const bool atmo_on =
      ocean_on || (atmosphere_ && atmosphere_->cloud_enabled());
  gpu_scene_.set_color_load_op(ocean_on ? render::rhi::ColorLoadOp::kLoad
                                        : render::rhi::ColorLoadOp::kClear);
  gpu_scene_.set_enable_depth(atmo_on);
  gpu_scene_.set_depth_load_op(ocean_on ? render::rhi::DepthLoadOp::kLoad
                                        : render::rhi::DepthLoadOp::kClear);
  if (!record_atmosphere_ocean(device, list, width_px, height_px, cam)) {
    device->destroy_command_list(list);
    return false;
  }
  if (!gpu_scene_.record_draws(device, list, width_px, height_px)) {
    device->destroy_command_list(list);
    return false;
  }
  if (!record_atmosphere_clouds(device, list, width_px, height_px, cam)) {
    device->destroy_command_list(list);
    return false;
  }
  list->close();
  const bool ok = device->execute(list);
  device->destroy_command_list(list);
  if (ok) {
    device->present();
  }
  return ok;
}

void Scene3dController::project(float x, float y, float z, int width_px,
                                int height_px, int* sx, int* sy) const {
  const float cy = std::cos(yaw_);
  const float syaw = std::sin(yaw_);
  const float cp = std::cos(pitch_);
  const float sp = std::sin(pitch_);
  const float x1 = x * cy - z * syaw;
  const float z1 = x * syaw + z * cy;
  const float y2 = y * cp - z1 * sp;
  const float z2 = y * sp + z1 * cp;
  const float depth = z2 + distance_;
  const float inv = depth > 0.15f ? (1.f / depth) : (1.f / 0.15f);
  const float f = 280.f * inv;
  if (sx) {
    *sx = width_px / 2 + static_cast<int>(std::lround(x1 * f));
  }
  if (sy) {
    *sy = height_px / 2 - static_cast<int>(std::lround(y2 * f));
  }
}

void Scene3dController::project_lon_lat(double lon, double lat, int width_px,
                                        int height_px, int* sx,
                                        int* sy) const {
  // Match DEM mesh framing: geographic X=-lon, Z=lat, then the same
  // center/scale as normalize_mesh over the active world extent.
  const content::Extent2 e = world_extent();
  const float minx = gis::dem_lon_to_x(e.xmax);  // xmax lon → more negative X
  const float maxx = gis::dem_lon_to_x(e.xmin);
  const float minz = static_cast<float>(e.ymin);
  const float maxz = static_cast<float>(e.ymax);
  const float cx = 0.5f * (minx + maxx);
  const float cz = 0.5f * (minz + maxz);
  const float span = (std::max)(maxx - minx, (std::max)(maxz - minz, 1.f));
  const float s = 3.2f / span;
  const float x = (gis::dem_lon_to_x(lon) - cx) * s;
  const float z = (static_cast<float>(lat) - cz) * s;
  project(x, 0.f, z, width_px, height_px, sx, sy);
}

void Scene3dController::paint_wind_arrows(HDC hdc, int width_px,
                                          int height_px) const {
  if (!hdc || !wind_overlay_enabled_ || !atmosphere_ || width_px <= 0 ||
      height_px <= 0) {
    return;
  }
  const gis::atmosphere::FieldStore& store = atmosphere_->field_store();
  if (store.layer_count() == 0) {
    return;
  }
  const content::Extent2 e = world_extent();
  const double lon_span = e.xmax - e.xmin;
  const double lat_span = e.ymax - e.ymin;
  if (!(lon_span > 0.0) || !(lat_span > 0.0)) {
    return;
  }

  constexpr int kGrid = 10;
  const double t = atmosphere_->time_sec();
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(120, 200, 255));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  for (int j = 0; j < kGrid; ++j) {
    for (int i = 0; i < kGrid; ++i) {
      const double lon =
          e.xmin + (static_cast<double>(i) + 0.5) / kGrid * lon_span;
      const double lat =
          e.ymin + (static_cast<double>(j) + 0.5) / kGrid * lat_span;
      const float u =
          store.sample(gis::atmosphere::FieldChannel::kWindU, lon, lat, t);
      const float v =
          store.sample(gis::atmosphere::FieldChannel::kWindV, lon, lat, t);
      const float speed = std::sqrt(u * u + v * v);
      if (!(speed > 1.0e-3f)) {
        continue;
      }
      int sx = 0;
      int sy = 0;
      project_lon_lat(lon, lat, width_px, height_px, &sx, &sy);
      if (sx < -20 || sy < -20 || sx > width_px + 20 || sy > height_px + 20) {
        continue;
      }
      // Arrow length scales with speed; cap so the grid stays readable.
      const float len = (std::min)(28.f, 6.f + speed * 1.2f);
      const float inv = 1.f / speed;
      const float dx = u * inv * len;
      const float dy = -v * inv * len;  // screen Y down; V is northward
      const int ex = sx + static_cast<int>(std::lround(dx));
      const int ey = sy + static_cast<int>(std::lround(dy));
      MoveToEx(hdc, sx, sy, nullptr);
      LineTo(hdc, ex, ey);
      // Simple arrowhead.
      const float hx = -dx * 0.25f;
      const float hy = -dy * 0.25f;
      const float px = -hy * 0.6f;
      const float py = hx * 0.6f;
      MoveToEx(hdc, ex, ey, nullptr);
      LineTo(hdc, ex + static_cast<int>(std::lround(hx + px)),
             ey + static_cast<int>(std::lround(hy + py)));
      MoveToEx(hdc, ex, ey, nullptr);
      LineTo(hdc, ex + static_cast<int>(std::lround(hx - px)),
             ey + static_cast<int>(std::lround(hy - py)));
    }
  }
  SelectObject(hdc, old_pen);
  DeleteObject(pen);
}

void Scene3dController::paint_hud(HDC hdc, int width_px, int height_px) const {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }
  remember_view_size(width_px, height_px);
  paint_wind_arrows(hdc, width_px, height_px);

  // Compass rose: needle points to geographic north on screen (default orbit
  // frames north toward the top of the viewport).
  {
    const int cx = width_px - 56;
    const int cy = 56;
    const int r = 28;
    HPEN ring = CreatePen(PS_SOLID, 2, RGB(210, 225, 240));
    HGDIOBJ old_pen = SelectObject(hdc, ring);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    const float angle = yaw_ - kScene3dDefaultYaw;
    const float nx = std::sin(angle);
    const float ny = -std::cos(angle);
    const int tip_x = cx + static_cast<int>(std::lround(nx * (r - 6)));
    const int tip_y = cy + static_cast<int>(std::lround(ny * (r - 6)));
    HPEN needle = CreatePen(PS_SOLID, 2, RGB(220, 60, 50));
    SelectObject(hdc, needle);
    MoveToEx(hdc, cx, cy, nullptr);
    LineTo(hdc, tip_x, tip_y);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(needle);
    DeleteObject(ring);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 245, 255));
    TextOutW(hdc, cx - 5, cy - r - 18, L"N", 1);
  }

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(220, 235, 250));
  wchar_t line[200];
  swprintf_s(line,
             L"Shared scene  yaw=%.2f pitch=%.2f dist=%.2f  "
             L"(wheel@cursor, pan, pinch)",
             yaw_, pitch_, distance_);
  TextOutW(hdc, 12, 12, line, lstrlenW(line));
  const wchar_t* so_t =
      hosts_shared_scene()
          ? L"Orbit DEM SoT — leftover SmartGis.exe is reference"
          : L"Local DEM SoT — leftover SmartGis.exe is reference";
  TextOutW(hdc, 12, 32, so_t, lstrlenW(so_t));
  const wchar_t* wasd = L"WASD  orbit / wheel zoom";
  TextOutW(hdc, 12, height_px > 40 ? height_px - 28 : 52, wasd, lstrlenW(wasd));
  if (atmosphere_) {
    wchar_t atmo[160];
    swprintf_s(atmo,
               L"Atmosphere  t=%.1fs  ocean=%d cloud=%d wind=%d",
               atmosphere_->time_sec(),
               atmosphere_->ocean_enabled() ? 1 : 0,
               atmosphere_->cloud_enabled() ? 1 : 0,
               wind_overlay_enabled_ ? 1 : 0);
    TextOutW(hdc, 12, 52, atmo, lstrlenW(atmo));
  }
}

namespace {

// Leftover SmartGis.exe hypsometric character: low green→yellow, high pink/white.
COLORREF hypsometric_rgb(float t01) {
  t01 = std::clamp(t01, 0.f, 1.f);
  int r = 0;
  int g = 0;
  int b = 0;
  if (t01 < 0.35f) {
    const float u = t01 / 0.35f;
    r = static_cast<int>(70 + 140 * u);
    g = static_cast<int>(140 + 70 * u);
    b = static_cast<int>(55 + 20 * (1.f - u));
  } else if (t01 < 0.65f) {
    const float u = (t01 - 0.35f) / 0.30f;
    r = static_cast<int>(210 + 25 * u);
    g = static_cast<int>(210 - 40 * u);
    b = static_cast<int>(75 + 40 * u);
  } else {
    const float u = (t01 - 0.65f) / 0.35f;
    r = static_cast<int>(235 + 20 * u);
    g = static_cast<int>(170 + 70 * u);
    b = static_cast<int>(115 + 120 * u);
  }
  return RGB(r, g, b);
}

}  // namespace

void Scene3dController::paint(HDC hdc, int width_px, int height_px,
                              bool fill_background) const {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }
  remember_view_size(width_px, height_px);

  // Heal sessions that already stored edge-on pitch (thin green strip).
  const_cast<Scene3dController*>(this)->pitch_ =
      std::clamp(pitch_, tool::kOrbitPitchMin, tool::kOrbitPitchMax);

  if (fill_background) {
    // Black void behind the ocean plane (leftover stereo SoT).
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  const_cast<Scene3dController*>(this)->rebuild_local_mesh();

  // Light-blue ocean / base plane under the DEM AABB (leftover character).
  if (!local_xyz_.empty()) {
    float minx = local_xyz_[0];
    float maxx = minx;
    float miny = local_xyz_[1];
    float maxy = miny;
    float minz = local_xyz_[2];
    float maxz = minz;
    for (size_t i = 0; i + 2 < local_xyz_.size(); i += 3) {
      minx = (std::min)(minx, local_xyz_[i]);
      maxx = (std::max)(maxx, local_xyz_[i]);
      miny = (std::min)(miny, local_xyz_[i + 1]);
      maxy = (std::max)(maxy, local_xyz_[i + 1]);
      minz = (std::min)(minz, local_xyz_[i + 2]);
      maxz = (std::max)(maxz, local_xyz_[i + 2]);
    }
    const float y_plane = miny - 0.02f * (std::max)(maxy - miny, 0.05f);
    int c[4][2] = {};
    project(minx, y_plane, minz, width_px, height_px, &c[0][0], &c[0][1]);
    project(maxx, y_plane, minz, width_px, height_px, &c[1][0], &c[1][1]);
    project(maxx, y_plane, maxz, width_px, height_px, &c[2][0], &c[2][1]);
    project(minx, y_plane, maxz, width_px, height_px, &c[3][0], &c[3][1]);
    const POINT ocean[4] = {{c[0][0], c[0][1]},
                            {c[1][0], c[1][1]},
                            {c[2][0], c[2][1]},
                            {c[3][0], c[3][1]}};
    HBRUSH ocean_br = CreateSolidBrush(RGB(120, 190, 230));
    HPEN ocean_pen = CreatePen(PS_SOLID, 1, RGB(90, 160, 210));
    HGDIOBJ old_pen = SelectObject(hdc, ocean_pen);
    HGDIOBJ old_brush = SelectObject(hdc, ocean_br);
    Polygon(hdc, ocean, 4);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(ocean_br);
    DeleteObject(ocean_pen);
  }

  // Continuous DEM: draw all tris up to a high cap. Sparse stride left
  // fragmented olive ribbons (not leftover hypsometric land).
  HPEN mesh_pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
  HGDIOBJ old_pen = SelectObject(hdc, mesh_pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  const size_t total_tris = local_idx_.size() / 3;
  constexpr size_t kMaxDraw = 24000;
  const size_t step =
      total_tris > kMaxDraw ? (total_tris + kMaxDraw - 1) / kMaxDraw : 1;

  float elev_min = 0.f;
  float elev_max = 0.f;
  bool elev_init = false;
  for (size_t i = 1; i + 2 < local_xyz_.size(); i += 3) {
    const float y = local_xyz_[i];
    if (!elev_init) {
      elev_min = elev_max = y;
      elev_init = true;
    } else {
      elev_min = (std::min)(elev_min, y);
      elev_max = (std::max)(elev_max, y);
    }
  }
  const float elev_span = (std::max)(elev_max - elev_min, 1.0e-3f);

  size_t drawn = 0;
  for (size_t t = 0; t < total_tris && drawn < kMaxDraw; t += step, ++drawn) {
    const unsigned i0 = local_idx_[t * 3];
    const unsigned i1 = local_idx_[t * 3 + 1];
    const unsigned i2 = local_idx_[t * 3 + 2];
    if ((i0 + 1) * 3 > local_xyz_.size() || (i1 + 1) * 3 > local_xyz_.size() ||
        (i2 + 1) * 3 > local_xyz_.size()) {
      continue;
    }
    const float y0 = local_xyz_[i0 * 3 + 1];
    const float y1 = local_xyz_[i1 * 3 + 1];
    const float y2 = local_xyz_[i2 * 3 + 1];
    const float yavg = (y0 + y1 + y2) / 3.f;
    const float t01 = (yavg - elev_min) / elev_span;
    HBRUSH fill = CreateSolidBrush(hypsometric_rgb(t01));
    SelectObject(hdc, fill);
    int p0[2] = {};
    int p1[2] = {};
    int p2[2] = {};
    project(local_xyz_[i0 * 3], local_xyz_[i0 * 3 + 1], local_xyz_[i0 * 3 + 2],
            width_px, height_px, &p0[0], &p0[1]);
    project(local_xyz_[i1 * 3], local_xyz_[i1 * 3 + 1], local_xyz_[i1 * 3 + 2],
            width_px, height_px, &p1[0], &p1[1]);
    project(local_xyz_[i2 * 3], local_xyz_[i2 * 3 + 1], local_xyz_[i2 * 3 + 2],
            width_px, height_px, &p2[0], &p2[1]);
    const POINT pts[3] = {{p0[0], p0[1]}, {p1[0], p1[1]}, {p2[0], p2[1]}};
    Polygon(hdc, pts, 3);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    DeleteObject(fill);
  }
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(mesh_pen);

  if (scene_) {
    scene_->paint_labels_projected(
        hdc, width_px, height_px,
        [this, width_px, height_px](double lon, double lat, int* sx, int* sy) {
          project_lon_lat(lon, lat, width_px, height_px, sx, sy);
        });
  }

  paint_hud(hdc, width_px, height_px);
}

}  // namespace app
