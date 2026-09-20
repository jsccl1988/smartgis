// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/scene3d_controller.h"

#include "app/views/map_scene.h"
#include "content/public/map_contents.h"
#include "gis/atmosphere/atmosphere_params.h"
#include "gis/atmosphere/cloud_system.h"
#include "gis/atmosphere/ocean_system.h"
#include "gis/world/dem_raster.h"
#include "gis/world/land_mask.h"
#include "gis/world/scene.h"
#include "render/atmosphere/ocean_pass.h"
#include "render/rhi/rhi.h"
#include "render/scene/scene.h"
#include "tool/camera_nav.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
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
  gpu_scene_.set_solid_color(0.62f, 0.70f, 0.48f, 1.f);
  const float aspect = static_cast<float>(width_px) /
                       static_cast<float>(height_px > 0 ? height_px : 1);
  const render::rhi::CameraMatrices cam = camera_matrices(aspect);
  gpu_scene_.set_view_camera(cam);

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

void Scene3dController::paint_hud(HDC hdc, int width_px, int height_px) const {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }
  remember_view_size(width_px, height_px);
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
          ? L"MapContents host — leftover SmartGis.exe is SoT"
          : L"Local DEM fallback — leftover SmartGis.exe is SoT";
  TextOutW(hdc, 12, 32, so_t, lstrlenW(so_t));
}

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
    HBRUSH bg = CreateSolidBrush(RGB(18, 32, 48));
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  const_cast<Scene3dController*>(this)->rebuild_local_mesh();
  // Filled facets (hypsometric-ish by elev). Mesh is row-major north→south;
  // drawing only the first N tris looked like a thin green ribbon. Stride
  // across the full index list so the China AABB stays visible under the cap.
  HPEN mesh_pen = CreatePen(PS_SOLID, 1, RGB(90, 120, 80));
  HGDIOBJ old_pen = SelectObject(hdc, mesh_pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  const size_t total_tris = local_idx_.size() / 3;
  constexpr size_t kMaxDraw = 2800;
  const size_t step =
      total_tris > kMaxDraw ? (total_tris + kMaxDraw - 1) / kMaxDraw : 1;
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
    // Normalized mesh is centered; map elev to green→brown.
    const float t01 = std::clamp(0.5f + yavg * 0.55f, 0.f, 1.f);
    const int r = static_cast<int>(70 + 110 * t01);
    const int g = static_cast<int>(120 + 40 * (1.f - t01));
    const int b = static_cast<int>(55 + 25 * (1.f - t01));
    HBRUSH fill = CreateSolidBrush(RGB(r, g, b));
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

  paint_hud(hdc, width_px, height_px);
  if (fill_background) {
    TextOutW(hdc, 12, 52, L"(GDI DEM mesh)", 15);
  }
}

}  // namespace app
