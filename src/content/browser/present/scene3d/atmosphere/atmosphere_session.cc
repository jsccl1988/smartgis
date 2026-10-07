// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/frame/tileset_stream.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include "content/browser/document/map_scene.h"
#include "vista/component/world/atmosphere/atmosphere_params.h"
#include "vista/component/world/atmosphere/cloud/cloud_system.h"
#include "vista/component/world/atmosphere/contour/contour_sheet.h"
#include "vista/component/world/atmosphere/field/field_channel.h"
#include "vista/component/world/atmosphere/field/field_ingest.h"
#include "vista/component/world/atmosphere/ocean/ocean_system.h"
#include "vista/terrain/dem/dem_contour.h"
#include "vista/assets/tileset/tileset.h"
#include "vista/terrain/dem/dem_frame.h"
#include "vista/terrain/dem/raster/dem_raster.h"
#include "vista/component/world/terrain/seed.h"
#include "vista/component/world/world.h"
#include "vista/pass/world/atmosphere/cloud/cloud_pass.h"
#include "vista/pass/world/atmosphere/fog/fog_pass.h"
#include "vista/pass/world/atmosphere/ocean/ocean_pass.h"
#include "vista/pass/world/atmosphere/sky/sky_pass.h"
#include "base/trace/event/process_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

namespace content {

AtmosphereSession::AtmosphereSession() {
  atmosphere_frame_.set_ocean_pass(&ocean_pass_);
  atmosphere_frame_.set_cloud_pass(&cloud_pass_);
  atmosphere_frame_.set_sky_pass(&sky_pass_);
  atmosphere_frame_.set_fog_pass(&fog_pass_);
  atmosphere_frame_.set_globe_pass(&globe_pass_);
  atmosphere_frame_.set_sat_cloud_pass(&sat_cloud_pass_);
}

AtmosphereSession::~AtmosphereSession() {
  release_passes();
}

void AtmosphereSession::bind_scene(const MapScene* scene) {
  if (scene_ != scene) {
    procedural_seeded_ = false;
    procedural_seed_scene_ = nullptr;
  }
  scene_ = scene;
}

void AtmosphereSession::bind_gpu(Scene3dGpuPresent* gpu) {
  gpu_ = gpu;
}

Extent2 AtmosphereSession::world_extent() const {
  // Reject Debug freefill pointers. Stale AtmosphereSession / Scene3dPresenter
  // layout across TUs can leave a non-null garbage gpu_ (e.g. 0xCDCDCD0000000000).
  const uintptr_t gpu_bits = reinterpret_cast<uintptr_t>(gpu_);
  const bool freefill =
      gpu_bits < 0x10000ull ||
      ((gpu_bits >> 32) & 0xFFFFFFFFull) == 0xCDCDCDCDull ||
      ((gpu_bits >> 32) & 0xFFFFFFFFull) == 0xCDCDCD00ull ||
      (gpu_bits & 0xFFFFFFFFull) == 0xCDCDCDCDull;
  if (gpu_ && !freefill) {
    return gpu_->world_extent();
  }
  return kChinaLonLatExtent;
}

bool AtmosphereSession::prepare_for_present() {
  BASE_TRACE_EVENT("atmosphere", "scene3d.atmosphere");
  // Best-effort: mesh rebuild usually already filled geo_frame; fill from
  // extent when atmosphere needs it before DEM sync.
  ensure_geo_frame();
  advance_sim_time();
  if (!prepare_globe() || !prepare_sat_clouds()) {
    return false;
  }
  // Globe stack owns land/ocean/weather on the sphere �?skip flat ocean/cloud.
  if (globe_enabled_) {
    return prepare_sky() && prepare_fog();
  }
  // POD prep only. OceanPass::prepare_gpu stays cold-once in Scene3dGpuPresent
  // (need_ocean_height_before_dem / dem_gpu_synced_after_ocean_); warm frames
  // must not call prepare_gpu from here.
  return prepare_ocean() && prepare_clouds() && prepare_sky() && prepare_fog();
}

void AtmosphereSession::advance_sim_time() {
  if (!atmosphere_) {
    return;
  }
  // Ocean FFT / cloud cover scrub need a moving clock; sky/fog are static
  // without it and look "frozen" in interactive present.
  if (!atmosphere_->ocean_enabled() && !atmosphere_->cloud_enabled() &&
      !atmosphere_->sky_enabled() && !sat_cloud_enabled_ && !globe_enabled_) {
    return;
  }
  LARGE_INTEGER qpc = {};
  QueryPerformanceCounter(&qpc);
  const std::uint64_t now = static_cast<std::uint64_t>(qpc.QuadPart);
  if (last_sim_qpc_ == 0) {
    last_sim_qpc_ = now;
    return;
  }
  LARGE_INTEGER freq = {};
  QueryPerformanceFrequency(&freq);
  const double hz = freq.QuadPart > 0 ? static_cast<double>(freq.QuadPart) : 1.0;
  double dt = static_cast<double>(now - last_sim_qpc_) / hz;
  last_sim_qpc_ = now;
  // Clamp spikes (tab switch / hitch) so waves do not jump.
  if (dt < 0.0) {
    dt = 0.0;
  } else if (dt > 0.1) {
    dt = 0.1;
  }
  if (dt <= 0.0) {
    return;
  }
  set_time_sec(atmosphere_->time_sec() + dt);
  // Dynamic sun: ~full azimuth revolution every ~120s; mild elevation bob so
  // DEM Lambert and ocean/sky specular read as living daylight.
  // Do not touch sea_mask_cache_* / overlay dirty �?coast mask is extent-keyed
  // and wave animation is OceanPass::record time, not a FieldStore resample.
  vista::atmosphere::AtmosphereParams& p = atmosphere_->params();
  const double t = atmosphere_->time_sec();
  constexpr double kTwoPi = 6.283185307179586;
  p.sun_azimuth_rad =
      static_cast<float>(std::fmod(0.55 + t * 0.0523598775598, kTwoPi));
  // Floor elevation so DEM Lambert / ocean specular never dip into the
  // magenta sunset band (prepare_sky also floors; keep light in sync).
  p.sun_elevation_rad =
      static_cast<float>(0.58 + 0.14 * std::sin(t * 0.041));
}

bool AtmosphereSession::needs_continuous_present() const {
  if (!atmosphere_) {
    return false;
  }
  return atmosphere_->ocean_enabled() || atmosphere_->cloud_enabled();
}

bool AtmosphereSession::ensure_geo_frame() {
  if (!gpu_) {
    return true;
  }
  if (!gpu_->geo_frame().valid) {
    gpu_->geo_frame() = OrbitGeoFrame::from_extent(world_extent());
  }
  return gpu_->geo_frame().valid;
}

void AtmosphereSession::release_passes() {
  ocean_pass_.release();
  cloud_pass_.release();
  sky_pass_.release();
  fog_pass_.release();
  globe_pass_.release();
  sat_cloud_pass_.release();
  globe_surface_loaded_ = false;
  sat_cloud_cover_loaded_ = false;
  sea_mask_cache_valid_ = false;
  cached_sea_mask_.clear();
  cached_sea_mask_n_ = 0;
}

vista::atmosphere::Environment& AtmosphereSession::ensure() {
  if (!atmosphere_) {
    atmosphere_ = vista::atmosphere::create_environment();
  }
  return *atmosphere_;
}

void AtmosphereSession::set_ocean_enabled(bool on) {
  ensure().set_ocean_enabled(on);
}

void AtmosphereSession::set_cloud_enabled(bool on) {
  ensure().set_cloud_enabled(on);
}

void AtmosphereSession::set_sky_enabled(bool on) {
  ensure().set_sky_enabled(on);
}

void AtmosphereSession::set_fog_enabled(bool on) {
  ensure().set_fog_enabled(on);
}

void AtmosphereSession::set_globe_enabled(bool on) {
  globe_enabled_ = on;
  atmosphere_frame_.set_globe_enabled(on);
  if (on) {
    // Globe owns land/ocean geometry; flat ocean/cloud would fight the sphere.
    ensure().set_ocean_enabled(false);
    ensure().set_cloud_enabled(false);
    ensure().set_sky_enabled(true);
  }
}

void AtmosphereSession::set_sat_cloud_enabled(bool on) {
  sat_cloud_enabled_ = on;
  atmosphere_frame_.set_sat_cloud_enabled(on);
}

void AtmosphereSession::set_elevation_overlay(bool surface, bool curves) {
  elevation_surface_overlay_ = surface;
  elevation_curve_overlay_ = curves;
  globe_pass_.set_elevation_overlay(surface, curves);
}

bool AtmosphereSession::elevation_surface_overlay() const {
  return elevation_surface_overlay_;
}

bool AtmosphereSession::elevation_curve_overlay() const {
  return elevation_curve_overlay_;
}

bool AtmosphereSession::apply_contour_suite_defaults() {
  vista::atmosphere::Environment& env = ensure();
  env.set_contour_enabled(true);
  vista::atmosphere::AtmosphereParams& p = env.params();
  p.contour_curves = true;
  p.contour_surface = true;
  p.contour_color_scale = true;
  // Mild elev scale: China-orbit DEM relief is ~0.3; keep the stacked sheet
  // readable without cliff-like undulation (see attach_tin contour slab).
  p.contour_dem_offset_m = 500.f;
  p.contour_value_to_meters = 160.f;
  p.contour_dem_vert_exag = 0.35f;
  // Opaque enough that ring ContourSheet hides wrinkled DEM facets beneath.
  p.contour_surface_alpha = 0.90f;

  vista::atmosphere::FieldGrid grid = field_grid();
  // Denser sample so the stacked sheet reads as smooth concentric bands.
  grid.cols = (std::max)(grid.cols, 96);
  grid.rows = (std::max)(grid.rows, 72);
  if (!env.rebuild_contour_sheet(vista::atmosphere::FieldChannel::kWaveHs, grid,
                                 nullptr)) {
    return false;
  }

  // ContourSheet carries jet+curves; do not stamp either onto globe albedo
  // (isoline/jet wash DEM facets under soft lighting �?chrome glare).
  set_elevation_overlay(false, false);

  if (globe_enabled_ || !gpu_) {
    return env.contour_sheet().has_color_scale() ||
           env.contour_sheet().has_surface();
  }

  const vista::atmosphere::ContourSheet& sheet = env.contour_sheet();
  if (!sheet.has_surface()) {
    return sheet.has_color_scale();
  }
  const vista::atmosphere::ContourSheetMesh& mesh = sheet.surface();
  const size_t nverts = mesh.xyz.size() / 3u;
  if (nverts < 3u || mesh.indices.size() < 3u) {
    return false;
  }
  std::vector<float> geo(nverts * 3u);
  for (size_t i = 0; i < nverts; ++i) {
    const float* vert = mesh.xyz.data() + i * 3u;
    geo[i * 3u + 0] = static_cast<float>(vista::dem_x_to_lon(vert[0]));
    geo[i * 3u + 1] = vert[2];
    geo[i * 3u + 2] = vert[1];
  }
  std::vector<unsigned> idx(mesh.indices.begin(), mesh.indices.end());
  // Do not clobber product overlay TINs (hex amber / mine lithology / cyan
  // stormsurge). Contour sheet is a diagnostic drape for bare China frames.
  if (gpu_->overlay_tin_has_albedo()) {
    const uint8_t* a = gpu_->overlay_tin_albedo();
    const bool hex_like =
        a[0] >= 160 && a[1] >= 100 && a[2] < 140 && a[0] > a[2] + 40;
    const bool water_like =
        a[2] >= 140 && a[1] >= 100 && a[0] < 90 && a[2] > a[0] + 40;
    const bool mine_like =
        a[0] >= 80 && a[1] >= 40 && a[2] >= 40 &&
        !(a[0] == 210 && a[1] == 220 && a[2] == 255);
    if (hex_like || water_like || mine_like) {
      return sheet.has_color_scale() || sheet.has_surface();
    }
  }
  // Alpha must stay at most 200 so attach_tin picks the soft ContourSheet
  // slab (0.16-0.38). Alpha>200 falls through to the hard 1.05-1.65 cliff
  // slab and the jet sheet reads as wrinkled folds (ui.scene).
  constexpr uint8_t kAlbedo[4] = {210, 220, 255, 190};
  gpu_->set_overlay_tin_mesh(geo.data(), static_cast<int>(nverts), idx.data(),
                             static_cast<int>(idx.size()), kAlbedo);

  if (mesh.rgba.size() >= nverts * 4u && mesh.uvs.size() >= nverts * 2u &&
      grid.cols >= 8 && grid.rows >= 8 &&
      nverts == static_cast<size_t>(grid.cols) * static_cast<size_t>(grid.rows)) {
    std::vector<uint8_t> atlas;
    std::vector<float> heights(nverts);
    for (size_t i = 0; i < nverts; ++i) {
      heights[i] = mesh.xyz[i * 3u + 1];
    }
    // Jet fill + dark charcoal isolines (paint.cc). White Origin stamps on
    // a 64x48 grid washed ui.scene; dark 1px strokes stay thin after upscale.
    if (!vista::bake_elevation_overlay_rgba(heights.data(), grid.cols,
                                            grid.rows, /*surface=*/true,
                                            /*curves=*/true, /*interval_m=*/0.f,
                                            &atlas) ||
        atlas.size() != nverts * 4u) {
      atlas.assign(nverts * 4u, 0);
      for (size_t i = 0; i < nverts; ++i) {
        const float* c = mesh.rgba.data() + i * 4u;
        atlas[i * 4u + 0] = static_cast<uint8_t>((std::max)(
            0, (std::min)(255, static_cast<int>(c[0] * 255.f + 0.5f))));
        atlas[i * 4u + 1] = static_cast<uint8_t>((std::max)(
            0, (std::min)(255, static_cast<int>(c[1] * 255.f + 0.5f))));
        atlas[i * 4u + 2] = static_cast<uint8_t>((std::max)(
            0, (std::min)(255, static_cast<int>(c[2] * 255.f + 0.5f))));
        atlas[i * 4u + 3] = 255;
      }
    } else {
      for (size_t i = 3; i < atlas.size(); i += 4) {
        if (atlas[i] != 0) {
          atlas[i] = 255;
        }
      }
    }
    gpu_->set_overlay_tin_drape(atlas.data(),
                                static_cast<uint32_t>(grid.cols),
                                static_cast<uint32_t>(grid.rows),
                                mesh.uvs.data(),
                                static_cast<int>(mesh.uvs.size()));
  } else if (sheet.has_color_scale()) {
    const auto& scale = sheet.color_scale();
    gpu_->set_overlay_tin_drape(scale.ramp_rgba.data(),
                                static_cast<uint32_t>(scale.ramp_w),
                                static_cast<uint32_t>(scale.ramp_h),
                                mesh.uvs.data(),
                                static_cast<int>(mesh.uvs.size()));
  }
  return true;
}

void AtmosphereSession::update_globe_detail_lod(float orbit_distance) {
  update_globe_detail_lod(orbit_distance, 0.0, 0.0);
}

void AtmosphereSession::update_globe_detail_lod(float orbit_distance,
                                               double look_lon_deg,
                                               double look_lat_deg) {
  if (!globe_enabled_ || !globe_surface_loaded_) {
    return;
  }
  // Match DemRaster::lod_max_edge overview (~3.2 R) and dem_seed_cache_key
  // near bucket. Full china_dem mix by skim (~1.65 R). Distances scale with
  // GlobeDrawParams.radius (default 4).
  const float R = (std::max)(1.f, globe_pass_.params().radius);
  // Keep space orbit (~2.6 R) above load so space BMP stays global_terrain.
  const float kChinaLoadDist = 2.4f * R;
  const float kChinaFullDist = 1.55f * R;
  float blend = 0.f;
  if (orbit_distance <= kChinaFullDist) {
    blend = 1.f;
  } else if (orbit_distance < kChinaLoadDist) {
    const float t = (kChinaLoadDist - orbit_distance) /
                    (kChinaLoadDist - kChinaFullDist);
    const float u = (std::max)(0.f, (std::min)(1.f, t));
    blend = u * u * (3.f - 2.f * u);
  }
  if (blend > 0.05f) {
    (void)load_china_globe_detail();
  }
  const float applied =
      globe_pass_.has_detail_surface() ? blend : 0.f;
  globe_pass_.set_detail_blend(applied);
  vista::GlobeDrawParams gp = globe_pass_.params();
  if (applied >= 0.55f) {
    gp.lon_slices = 512;
    gp.lat_slices = 256;
  } else if (applied >= 0.15f || globe_pass_.dem_is_global()) {
    gp.lon_slices = 448;
    gp.lat_slices = 224;
  } else {
    gp.lon_slices = 320;
    gp.lat_slices = 160;
  }
  globe_pass_.set_params(gp);

  if (look_lon_deg != 0.0 || look_lat_deg != 0.0) {
    apply_globe_sea_ocean(look_lon_deg, look_lat_deg, /*on=*/true);
  }
}

void AtmosphereSession::apply_globe_sea_ocean(double lon_deg, double lat_deg,
                                              bool on) {
  if (!on || !globe_enabled_) {
    return;
  }
  // Open water: DEM height near sea level (East China Sea skim).
  const float h = globe_pass_.height_meters(lon_deg, lat_deg);
  if (h < 8.f) {
    ensure().set_ocean_enabled(true);
  }
}

void AtmosphereSession::set_wind_overlay_enabled(bool on) {
  wind_overlay_enabled_ = on;
  if (on) {
    // Need WindU/V samples for arrows; seed procedural if store empty.
    vista::atmosphere::Environment& env = ensure();
    if (env.field_store().layer_count() == 0) {
      seed_procedural();
    }
  }
}

void AtmosphereSession::set_time_sec(double t) {
  vista::atmosphere::Environment& env = ensure();
  env.scrub_time_sec(t);
  // Prefer clamp against External wave/cover/wind if a timed range exists.
  static const vista::atmosphere::FieldChannel kClampOrder[] = {
      vista::atmosphere::FieldChannel::kWaveHs,
      vista::atmosphere::FieldChannel::kCloudCover,
      vista::atmosphere::FieldChannel::kWindU,
      vista::atmosphere::FieldChannel::kWindV,
      vista::atmosphere::FieldChannel::kWaveDir,
      vista::atmosphere::FieldChannel::kCloudBase,
      vista::atmosphere::FieldChannel::kCloudTop,
      vista::atmosphere::FieldChannel::kSeaMask,
  };
  for (vista::atmosphere::FieldChannel ch : kClampOrder) {
    if (env.clamp_time_to_field(ch)) {
      break;
    }
  }
}

double AtmosphereSession::time_sec() const {
  return atmosphere_ ? atmosphere_->time_sec() : 0.0;
}

namespace {

vista::atmosphere::FieldChannel parse_field_channel(std::string_view name,
                                                  bool* ok) {
  *ok = true;
  if (name == "wind_u" || name == "u") {
    return vista::atmosphere::FieldChannel::kWindU;
  }
  if (name == "wind_v" || name == "v") {
    return vista::atmosphere::FieldChannel::kWindV;
  }
  if (name == "wave_hs" || name == "hs") {
    return vista::atmosphere::FieldChannel::kWaveHs;
  }
  if (name == "wave_dir" || name == "dir") {
    return vista::atmosphere::FieldChannel::kWaveDir;
  }
  if (name == "cloud_cover" || name == "cover" || name == "cloud") {
    return vista::atmosphere::FieldChannel::kCloudCover;
  }
  if (name == "cloud_base" || name == "base") {
    return vista::atmosphere::FieldChannel::kCloudBase;
  }
  if (name == "cloud_top" || name == "top") {
    return vista::atmosphere::FieldChannel::kCloudTop;
  }
  if (name == "sea_mask" || name == "sea") {
    return vista::atmosphere::FieldChannel::kSeaMask;
  }
  *ok = false;
  return vista::atmosphere::FieldChannel::kCloudCover;
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

bool AtmosphereSession::load_fields(std::string_view spec) {
  // Parse path[:channel[:time]][,...] then batch by channel into
  // Environment::load_external_series (ingest_gdal_field_series).
  spec = trim_ascii(spec);
  if (spec.empty()) {
    return false;
  }
  vista::atmosphere::Environment& env = ensure();

  // Preserve input order of first appearance per channel.
  std::vector<vista::atmosphere::FieldChannel> channel_order;
  std::vector<std::vector<FieldSeriesEntry>> series_by_channel(
      static_cast<std::size_t>(vista::atmosphere::FieldChannel::kCount));

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
    vista::atmosphere::FieldChannel channel =
        vista::atmosphere::FieldChannel::kCloudCover;
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

  vista::atmosphere::FieldIngestOptions opts;
  opts.kind = vista::atmosphere::FieldSourceKind::kExternal;
  opts.priority = 20;

  bool any = false;
  vista::atmosphere::FieldChannel first_ok =
      vista::atmosphere::FieldChannel::kCloudCover;
  for (vista::atmosphere::FieldChannel channel : channel_order) {
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

vista::atmosphere::FieldGrid AtmosphereSession::field_grid() const {
  const content::Extent2 e = world_extent();
  vista::atmosphere::FieldGrid grid;
  grid.min_lon = e.xmin;
  grid.min_lat = e.ymin;
  grid.max_lon = e.xmax;
  grid.max_lat = e.ymax;
  grid.cols = 32;
  grid.rows = 32;
  return grid;
}

void AtmosphereSession::seed_procedural() {
  seed_procedural(true);
}

namespace {

// When MapScene has no land rings, derive kSeaMask from china_dem so ocean
// stays around the mainland instead of painting a full-screen black patch.
bool seed_sea_mask_from_dem(vista::atmosphere::Environment& env,
                            const vista::atmosphere::FieldGrid& grid) {
  if (grid.empty()) {
    return false;
  }
  vista::DemRaster dem;
  const std::string path = vista::find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    // Real-data policy: no synthetic China DEM stand-in.
    return false;
  }
  double dem_minx = 0;
  double dem_miny = 0;
  double dem_maxx = 0;
  double dem_maxy = 0;
  dem.envelope(&dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
  std::vector<float> sea(grid.cell_count(), 1.f);
  for (int row = 0; row < grid.rows; ++row) {
    for (int col = 0; col < grid.cols; ++col) {
      double lon = grid.min_lon;
      double lat = grid.min_lat;
      if (grid.cols > 1) {
        lon = grid.min_lon +
              (grid.max_lon - grid.min_lon) *
                  (static_cast<double>(col) /
                   static_cast<double>(grid.cols - 1));
      }
      if (grid.rows > 1) {
        lat = grid.min_lat +
              (grid.max_lat - grid.min_lat) *
                  (static_cast<double>(row) /
                   static_cast<double>(grid.rows - 1));
      }
      const std::size_t idx =
          static_cast<std::size_t>(row) *
              static_cast<std::size_t>(grid.cols) +
          static_cast<std::size_t>(col);
      const bool inside = lon >= dem_minx && lon <= dem_maxx &&
                          lat >= dem_miny && lat <= dem_maxy;
      // Match DemRaster land rebuild: elev > 1 m �?land (sea mask 0).
      const bool land = inside && dem.sample_meters(lon, lat) > 1.f;
      sea[idx] = land ? 0.f : 1.f;
    }
  }
  vista::atmosphere::FieldLayer layer;
  layer.channel = vista::atmosphere::FieldChannel::kSeaMask;
  layer.kind = vista::atmosphere::FieldSourceKind::kProcedural;
  layer.priority = 1;  // Prefer over empty-ring fail-closed layer.
  layer.grid = grid;
  layer.values = std::move(sea);
  // Timeless procedural mask �?default time_sec=0 would invent a timed range
  // and clamp AtmosphereSession::set_time_sec (scene3d time scrub).
  layer.time_sec = std::numeric_limits<double>::quiet_NaN();
  env.field_store().set_layer(layer);
  return true;
}

}  // namespace

void AtmosphereSession::seed_procedural(bool with_land_rings) {
  // Tab switch / product defaults call this often; re-exporting china land
  // rings every time dominates interactive Scene3D cost.
  if (procedural_seeded_ && procedural_seed_with_rings_ == with_land_rings &&
      procedural_seed_scene_ == scene_) {
    return;
  }
  vista::atmosphere::Environment& env = ensure();
  std::vector<vista::LonLatRing> rings;
  if (with_land_rings && scene_) {
    scene_->export_land_rings(&rings);
  }
  const vista::atmosphere::FieldGrid grid = field_grid();
  env.seed_procedural_baseline(grid, rings.empty() ? nullptr : &rings);
  if (rings.empty()) {
    (void)seed_sea_mask_from_dem(env, grid);
  }
  sea_mask_cache_valid_ = false;
  env.sync_systems_from_params();
  procedural_seeded_ = true;
  procedural_seed_with_rings_ = with_land_rings;
  procedural_seed_scene_ = scene_;
}

void AtmosphereSession::enable_demo() {
  vista::atmosphere::Environment& env = ensure();
  std::vector<vista::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  const vista::atmosphere::FieldGrid grid = field_grid();
  env.enable_demo(grid, rings.empty() ? nullptr : &rings);
  if (rings.empty()) {
    (void)seed_sea_mask_from_dem(env, grid);
  }
  sea_mask_cache_valid_ = false;
}

namespace {

bool m3_resolve_stub(const char* uri, vista::ModelAsset* out, size_t* byte_cost,
                     void* user) {
  (void)user;
  if (!uri || !out || !byte_cost) {
    return false;
  }
  // Intentionally missing content â?streamer must degrade (AABB / stub).
  if (std::strcmp(uri, "missing.glb") == 0) {
    return false;
  }
  out->name = uri;
  out->meshes.clear();
  *byte_cost = 512;
  return true;
}

bool m3_fail(std::string* err, const char* tag) {
  if (err) {
    *err = tag ? tag : "m3-fail";
  }
  return false;
}

}  // namespace

bool AtmosphereSession::run_m3_self_test_hooks(std::string* err) {
  // --- m3-dem-ok: china DEM (or synthetic) seeds a terrain mesh with height.
  {
    vista::World dem_world;
    vista::Node* dem =
        vista::seed_china_dem_into_world(&dem_world, nullptr, 0, "m3_dem", 48);
    if (!dem || !dem->has_terrain_mesh()) {
      return m3_fail(err, "m3-dem-ok");
    }
    bool height_signal = false;
    const std::vector<float>& pos = dem->terrain.positions;
    for (size_t i = 1; i + 1 < pos.size(); i += 3) {
      if (std::fabs(pos[i]) > 1e-5f) {
        height_signal = true;
        break;
      }
    }
    if (!height_signal) {
      return m3_fail(err, "m3-dem-ok");
    }
  }

  // --- m3-tiles-ok: fixture tileset â?select/stream + content cache cap.
  {
    // City-scale fixture (also shipped as testing/data/m3_city_tileset.json).
    const char* ts_json =
        "{\"asset\":{\"version\":\"1.1\"},\"root\":{"
        "\"boundingVolume\":{\"region\":[2.0,0.6,2.1,0.7,0,200]},"
        "\"geometricError\":64,\"refine\":\"REPLACE\","
        "\"content\":{\"uri\":\"city_root.glb\"},"
        "\"children\":["
        "{\"boundingVolume\":{\"region\":[2.0,0.6,2.05,0.65,0,120]},"
        "\"geometricError\":0,\"content\":{\"uri\":\"city_a.glb\"}},"
        "{\"boundingVolume\":{\"region\":[2.05,0.65,2.1,0.7,0,120]},"
        "\"geometricError\":0,\"content\":{\"uri\":\"city_b.glb\"}},"
        "{\"boundingVolume\":{\"region\":[2.02,0.62,2.08,0.68,0,80]},"
        "\"geometricError\":0,\"content\":{\"uri\":\"missing.glb\"}}"
        "]}}";
    vista::Tileset tileset;
    if (!vista::parse_tileset_json(ts_json, std::strlen(ts_json), tileset)) {
      return m3_fail(err, "m3-tiles-ok");
    }
    vista::World world;
    vista::Node* node = world.attach_tileset(&tileset, "m3_city");
    if (!node) {
      return m3_fail(err, "m3-tiles-ok");
    }
    vista::ViewState view;
    view.eye_x = 2.05;
    view.eye_y = 0.65;
    view.eye_z = 0.05;
    view.sse_denominator = 1;
    if (!world.stream_tileset(node->id, view, 0, 8)) {
      return m3_fail(err, "m3-tiles-ok");
    }
    const vista::Node* streamed = world.find(node->id);
    if (!streamed || streamed->visible_uris.empty()) {
      return m3_fail(err, "m3-tiles-ok");
    }

    vista::TilesetContentCache cache(600);
    std::vector<const vista::Tile*> visible;
    vista::select_tiles(tileset, view, 0, visible);
    vista::ensure_tileset_content(visible, &cache, m3_resolve_stub, nullptr);
    if (cache.resident_bytes() > cache.max_bytes()) {
      return m3_fail(err, "m3-tiles-ok");
    }
    if (!cache.contains("missing.glb")) {
      return m3_fail(err, "m3-tiles-ok");
    }
    const vista::TilesetContentEntry* miss = cache.try_get("missing.glb");
    if (!miss || miss->decode_ok) {
      return m3_fail(err, "m3-tiles-ok");
    }
    // Second put of another leaf must stay under budget (LRU eviction).
    vista::ModelAsset extra;
    extra.name = "extra";
    if (!cache.put("city_a.glb", extra, 512, true) &&
        cache.resident_bytes() > cache.max_bytes()) {
      return m3_fail(err, "m3-tiles-ok");
    }
    if (cache.resident_bytes() > cache.max_bytes()) {
      return m3_fail(err, "m3-tiles-ok");
    }

    // Present-path stream session: camera move changes visible_uris; cache
    // stays under budget; missing URI degrades without crash.
    {
      content::TilesetStreamSession stream;
      vista::World present_world;
      if (!stream.attach_json(&present_world, ts_json, std::strlen(ts_json),
                              "m3_present")) {
        return m3_fail(err, "m3-tiles-ok");
      }
      vista::ViewState near_view = view;
      near_view.eye_z = 0.02;
      stream.pump_view(&present_world, near_view, 0, 8);
      const std::vector<std::string> uris_a = stream.last_visible_uris();
      if (uris_a.empty()) {
        return m3_fail(err, "m3-tiles-ok");
      }
      vista::ViewState far_view = view;
      far_view.eye_z = 50.0;
      stream.pump_view(&present_world, far_view, 1e6, 8);
      const std::vector<std::string> uris_b = stream.last_visible_uris();
      if (uris_b.empty()) {
        return m3_fail(err, "m3-tiles-ok");
      }
      if (uris_a == uris_b && uris_a.size() > 1) {
        // High SSE should collapse toward root; tolerate equal only for
        // single-URI selections.
        return m3_fail(err, "m3-tiles-ok");
      }
      if (stream.cache().resident_bytes() > stream.cache().max_bytes()) {
        return m3_fail(err, "m3-tiles-ok");
      }
    }
  }

  // --- m3-atmosphere-ok: demo on (ocean/cloud/sky/fog), then all off.
  {
    enable_demo();
    const vista::atmosphere::Environment* env = environment();
    if (!env || !env->ocean_enabled() || !env->cloud_enabled() ||
        !env->sky_enabled() || !env->fog_enabled()) {
      return m3_fail(err, "m3-atmosphere-ok");
    }
    set_ocean_enabled(false);
    set_cloud_enabled(false);
    set_sky_enabled(false);
    set_fog_enabled(false);
    if (env->ocean_enabled() || env->cloud_enabled() || env->sky_enabled() ||
        env->fog_enabled()) {
      return m3_fail(err, "m3-atmosphere-ok");
    }
  }

  if (err) {
    err->clear();
  }
  return true;
}


bool AtmosphereSession::prepare_ocean() {
  if (!atmosphere_ || !atmosphere_->ocean_enabled()) {
    return true;
  }
  if (!gpu_ || !gpu_->geo_frame().valid) {
    return true;
  }
  atmosphere_->sync_systems_from_params();

  const content::Extent2 e = gpu_->geo_frame().extent;
  vista::atmosphere::FieldGrid extent;
  extent.min_lon = e.xmin;
  extent.min_lat = e.ymin;
  extent.max_lon = e.xmax;
  extent.max_lat = e.ymax;
  extent.cols = 1;
  extent.rows = 1;

  const vista::atmosphere::OceanTileParams tile =
      atmosphere_->ocean_system().sample_tile(
          atmosphere_->field_store(), extent, atmosphere_->time_sec());

  float min_x = 0.f;
  float max_x = 0.f;
  float min_z = 0.f;
  float max_z = 0.f;
  gpu_->geo_frame().extent_orbit_xz(&min_x, &max_x, &min_z, &max_z);
  const float pad = OrbitGeoFrame::kOceanPad;
  min_x -= pad;
  max_x += pad;
  min_z -= pad;
  max_z += pad;

  vista::OceanDrawParams draw;
  // Wave height: GIS meters �?orbit Y (same scale as DEM elev).
  draw.significant_wave_height = (std::max)(
      0.01f, gpu_->geo_frame().meters_to_orbit_y(tile.spectrum.significant_wave_height));
  draw.mean_direction_rad = tile.spectrum.mean_direction_rad;
  draw.wind_speed = tile.spectrum.wind_speed;
  draw.wind_direction_rad = tile.spectrum.wind_direction_rad;
  draw.fft_size = tile.spectrum.fft_size;
  draw.use_gerstner_fallback = tile.spectrum.use_gerstner_fallback;
  draw.use_jonswap = tile.spectrum.use_jonswap;
  draw.chop = tile.spectrum.chop;
  draw.jonswap_gamma = tile.spectrum.jonswap_gamma;
  draw.patch_center_x = 0.5f * (min_x + max_x);
  draw.patch_center_z = 0.5f * (min_z + max_z);
  draw.patch_y = gpu_->geo_frame().sea_level_y();
  draw.patch_half_x = 0.5f * (max_x - min_x);
  draw.patch_half_z = 0.5f * (max_z - min_z);
  draw.patch_half_extent =
      (std::max)(draw.patch_half_x, draw.patch_half_z);
  // China orbit span �?3.2: keep a readable lip without swallowing DEM peaks.
  // Prefer Gerstner for interactive full-China (GPU FFT can look flat when the
  // height-map energy is tiny after 1/N² at this scale).
  const bool legacy_stereo =
      gpu_->look_preset() == Scene3dLookPreset::kLegacyStereo;
  if (legacy_stereo) {
    // Leftover stereo: calm light-blue shelf on black clear. Hs�?.10 used to
    // submerge coastal/mid DEM (only a mountain strip survived ocean depth).
    draw.significant_wave_height =
        (std::min)((std::max)(draw.significant_wave_height, 0.01f), 0.035f);
    draw.use_gerstner_fallback = true;
    draw.prefer_gpu_fft = false;
    draw.chop = (std::min)((std::max)(draw.chop, 0.35f), 0.55f);
    draw.shininess = (std::max)(draw.shininess, 120.0f);
    draw.mesh_resolution = 33;
    draw.deep_r = 0.01f;
    draw.deep_g = 0.02f;
    draw.deep_b = 0.04f;
    draw.shallow_r = 0.22f;
    draw.shallow_g = 0.48f;
    draw.shallow_b = 0.62f;
    draw.fresnel_bias = 0.04f;
    draw.fresnel_power = 5.0f;
  } else {
    draw.significant_wave_height =
        (std::max)((std::min)(draw.significant_wave_height * 4.5f, 0.22f), 0.10f);
    draw.use_gerstner_fallback = true;
    draw.prefer_gpu_fft = false;
    draw.chop = (std::max)(draw.chop, 1.15f);
    draw.shininess = (std::max)(draw.shininess, 180.0f);
    // 33 matches OceanDrawParams default; 65² Gerstner+upload dominated present.
    draw.mesh_resolution = 33;
    // Mid-tier GIS water: deep navy, muted shelf �?never near-cyan albedo.
    draw.deep_r = 0.02f;
    draw.deep_g = 0.07f;
    draw.deep_b = 0.18f;
    draw.shallow_r = 0.06f;
    draw.shallow_g = 0.22f;
    draw.shallow_b = 0.32f;
    draw.fresnel_bias = 0.03f;
    draw.fresnel_power = 6.0f;
  }
  ocean_pass_.set_params(draw);
  const vista::atmosphere::AtmosphereParams& p = atmosphere_->params();
  ocean_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                             p.sun_elevation_rad);
  ocean_pass_.set_time_sec(atmosphere_->time_sec());

  constexpr int kMask = 32;
  const Extent2 frame_extent = gpu_->geo_frame().extent;
  const bool extent_changed =
      !sea_mask_cache_valid_ || cached_sea_mask_n_ != kMask ||
      cached_sea_mask_extent_.xmin != frame_extent.xmin ||
      cached_sea_mask_extent_.ymin != frame_extent.ymin ||
      cached_sea_mask_extent_.xmax != frame_extent.xmax ||
      cached_sea_mask_extent_.ymax != frame_extent.ymax;
  if (extent_changed) {
    // Start at 0 (land). Never pre-fill 1 �?a failed/partial fill used to
    // leave a full-screen sea mask and black out China DEM.
    cached_sea_mask_.assign(
        static_cast<std::size_t>(kMask) * static_cast<std::size_t>(kMask), 0.f);
    extent.cols = kMask;
    extent.rows = kMask;
    // Extent-keyed only: advance_sim_time must not reach this branch.
    if (!atmosphere_->ocean_system().fill_sea_mask_grid(
            atmosphere_->field_store(), extent, kMask, kMask,
            atmosphere_->time_sec(), cached_sea_mask_.data(),
            cached_sea_mask_.size())) {
      // Fail closed: no sea_mask layer means do not cover the DEM with a
      // full-screen ocean (mask=1). Interactive 3D used to open blank cyan
      // when seed_procedural was skipped (has_china_extent false).
      std::fill(cached_sea_mask_.begin(), cached_sea_mask_.end(), 0.f);
    }
    cached_sea_mask_extent_ = frame_extent;
    cached_sea_mask_n_ = kMask;
    sea_mask_cache_valid_ = true;
  }
  // Cache hit: rebind is a no-op when OceanPass still holds the same bytes
  // (memcmp). After OceanPass::release the cache still avoids FieldStore work.
  if (sea_mask_cache_valid_ && !cached_sea_mask_.empty()) {
    ocean_pass_.set_sea_mask_cpu(kMask, kMask, cached_sea_mask_.data(),
                                 cached_sea_mask_.size());
  }
  return true;
}

bool AtmosphereSession::prepare_clouds() {
  if (!atmosphere_ || !atmosphere_->cloud_enabled()) {
    return true;
  }
  if (!gpu_ || !gpu_->geo_frame().valid) {
    return true;
  }
  const content::Extent2 e = gpu_->geo_frame().extent;
  const double clon = 0.5 * (e.xmin + e.xmax);
  const double clat = 0.5 * (e.ymin + e.ymax);
  const vista::atmosphere::CloudSample sample =
      atmosphere_->cloud_system().sample_at(
          atmosphere_->field_store(), clon, clat, atmosphere_->time_sec());

  const vista::atmosphere::AtmosphereParams& p = atmosphere_->params();
  cloud_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                             p.sun_elevation_rad);
  cloud_pass_.set_cloud_slab(sample.base_m, sample.top_m);
  // Broken deck: visible but soft enough that landish greens survive.
  cloud_pass_.set_cover_modulation(
      (std::max)(0.38f, (std::min)(sample.cover, 0.62f)));

  float min_x = 0.f;
  float max_x = 0.f;
  float min_z = 0.f;
  float max_z = 0.f;
  gpu_->geo_frame().extent_orbit_xz(&min_x, &max_x, &min_z, &max_z);
  const float half_x = 0.5f * (max_x - min_x) + OrbitGeoFrame::kOceanPad;
  const float half_z = 0.5f * (max_z - min_z) + OrbitGeoFrame::kOceanPad;
  // Keep a thin deck just above the terrain (full GIS cloud base would sit
  // several orbit-units up as a gray card).
  const float sea = gpu_->geo_frame().sea_level_y();
  float lift = gpu_->geo_frame().meters_to_orbit_y(sample.base_m);
  lift = (std::max)(0.22f, (std::min)(lift, 0.62f));
  float thick = gpu_->geo_frame().meters_to_orbit_y(sample.top_m - sample.base_m);
  thick = (std::max)(0.16f, (std::min)(thick, 0.36f));
  const float base_y = sea + lift;
  const float top_y = base_y + thick;
  const float deck_y = 0.5f * (base_y + top_y);
  cloud_pass_.set_deck_orbit(half_x, half_z, deck_y);
  cloud_pass_.set_slab_orbit(base_y, top_y);
  return true;
}

bool AtmosphereSession::prepare_sky() {
  if (!atmosphere_ || !atmosphere_->sky_enabled()) {
    return true;
  }
  const vista::atmosphere::AtmosphereParams& p = atmosphere_->params();
  // Floor sun elevation so daytime China orbit never samples a magenta
  // sunset mid-band (dynamic bob can dip to ~0.10 rad).
  const float sun_el = (std::max)(p.sun_elevation_rad, 0.72f);
  sky_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad, sun_el);
  vista::SkyDrawParams sky = sky_pass_.params();
  // Past max orbit distance (12) so zoom-out stays inside the sky.
  sky.dome_radius = 40.0f;
  if (globe_enabled_) {
    // Product splash / Google-Earth path: deep-space starfield, not Rayleigh.
    // Negative dome_radius selects space_blend in SkyPass (no POD growth).
    sky.dome_radius = -40.0f;
    sky.zenith_r = 0.008f;
    sky.zenith_g = 0.010f;
    sky.zenith_b = 0.028f;
    sky.horizon_r = 0.012f;
    sky.horizon_g = 0.014f;
    sky.horizon_b = 0.040f;
    sky.sunset_r = sky.horizon_r;
    sky.sunset_g = sky.horizon_g;
    sky.sunset_b = sky.horizon_b;
    sky.sun_glow_strength = 0.55f;
  } else {
    // Industry mid-tier analytical dome: deep Rayleigh zenith, cool haze
    // horizon. Collapse sunset into horizon so screen-space ground never
    // samples a magenta sunset leg (interactive no-arg 3D lower half).
    sky.dome_radius = 40.0f;
    sky.zenith_r = 0.04f;
    sky.zenith_g = 0.16f;
    sky.zenith_b = 0.86f;
    sky.horizon_r = 0.42f;
    sky.horizon_g = 0.64f;
    sky.horizon_b = 0.90f;
    sky.sunset_r = sky.horizon_r;
    sky.sunset_g = sky.horizon_g;
    sky.sunset_b = sky.horizon_b;
    sky.sun_glow_strength = 0.18f;
  }
  sky_pass_.set_params(sky);
  return true;
}

bool AtmosphereSession::prepare_fog() {
  if (!atmosphere_ || !atmosphere_->fog_enabled()) {
    return true;
  }
  if (!gpu_ || !gpu_->geo_frame().valid) {
    return true;
  }
  const vista::atmosphere::AtmosphereParams& p = atmosphere_->params();
  vista::FogDrawParams fog;
  // Soft aerial haze on terrain only (sky depth is skipped in FogPass HLSL).
  // Cap opacity so hypsometric greens still pass showcase landish gates.
  fog.density = (std::max)((std::min)(p.fog_density, 0.08f), 0.03f);
  // China orbit frame span ~3.2: keep haze visible without washing terrain.
  fog.visibility = (std::max)(2.8f, (std::min)(p.fog_visibility, 5.0f));
  fog.height_falloff = p.fog_height_falloff;
  fog.max_opacity = (std::min)((std::max)(p.fog_max_opacity, 0.06f), 0.12f);
  fog.base_height = gpu_->geo_frame().sea_level_y();
  // Tint haze toward the analytical sky horizon (matches sky pass).
  // Zero sun_glow for the tint sample �?a fixed horizon ray can align with
  // the sun and pick up disk/corona, blowing fog to near-white and washing
  // the showcase BMP (blue_sky / landish gates fail).
  float hr = fog.color_r;
  float hg = fog.color_g;
  float hb = fog.color_b;
  vista::SkyDrawParams tint = sky_pass_.params();
  tint.sun_glow_strength = 0.f;
  vista::SkyPass::sample_sky_rgb(tint, 0.f, 0.05f, 1.f, &hr, &hg,
                                              &hb);
  fog.color_r = hr;
  fog.color_g = hg;
  fog.color_b = hb;
  fog_pass_.set_params(fog);
  return true;
}

namespace {

std::string find_sample_sat_cloud_path() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  std::string dir;
  if (n > 0 && n < MAX_PATH) {
    dir.assign(module, module + n);
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
      dir.resize(slash + 1);
    }
  }
  const char* rel[] = {
      "..\\data\\sat_cloud.tif",
      "..\\data\\sat_cloud.png",
      "..\\data\\global_cloud.tif",
      "..\\data\\satellite_cloud.tif",
      "data\\sat_cloud.tif",
      "testing\\data\\sat_cloud.tif",
  };
  for (const char* r : rel) {
    std::string cand = dir;
    cand += r;
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

}  // namespace

bool AtmosphereSession::load_china_globe_detail() {
  if (china_globe_detail_loaded_) {
    return globe_pass_.has_detail_surface();
  }
  china_globe_detail_loaded_ = true;
  if (!globe_pass_.dem_is_global()) {
    // Base surface is already the China window �?no second overlay.
    return false;
  }
  vista::DemRaster dem;
  const std::string path = vista::find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    return false;
  }
  double minx = 73.0;
  double miny = 18.0;
  double maxx = 135.0;
  double maxy = 54.0;
  dem.envelope(&minx, &miny, &maxx, &maxy);
  const bool looks_global =
      (minx <= -170.0 && maxx >= 170.0 && miny <= -80.0 && maxy >= 80.0);
  if (looks_global) {
    return false;
  }
  if (maxx <= minx || maxy <= miny) {
    minx = 73.0;
    miny = 18.0;
    maxx = 135.0;
    maxy = 54.0;
  }
  dem.fit_vertical_exaggeration();
  // Hypsometric from china_dem heights — global_terrain in the China window
  // painted the DEM-hug BMP as solid ocean cyan (no land read). Soft Lambert
  // in ps_globe keeps peaks matte (no chrome). Space orbit still uses
  // global_terrain via the base equirect (detail_blend=0 above ~2.4 R).
  std::vector<float> heights;
  std::vector<uint8_t> rgba;
  int cols = 0;
  int rows = 0;
  int tw = 0;
  int th = 0;
  bool imagery_loaded = false;
  if (!dem.sample_globe_surface(minx, miny, maxx, maxy, false, nullptr,
                                &heights, &cols, &rows, &rgba, &tw, &th,
                                &imagery_loaded)) {
    return false;
  }
  // Drop sampled albedo so GlobePass bakes hypsometric land (green/tan) from
  // detail heights; empty albedo triggers the height→color path.
  rgba.clear();
  tw = 0;
  th = 0;
  imagery_loaded = false;
  globe_pass_.set_detail_dem_surface(
      minx, miny, maxx, maxy, cols, rows,
      heights.empty() ? nullptr : heights.data(), heights.size(),
      nullptr, 0, 0);
  std::fprintf(stderr,
               "atmosphere.globe: china_detail=%s envelope=[%.1f,%.1f]-"
               "[%.1f,%.1f] albedo=%s\n",
               path.c_str(), minx, miny, maxx, maxy,
               imagery_loaded ? "(imagery)" : "(hypsometric)");
  std::fflush(stderr);
  return globe_pass_.has_detail_surface();
}

bool AtmosphereSession::prepare_globe() {
  atmosphere_frame_.set_globe_enabled(globe_enabled_);
  if (!globe_enabled_) {
    return true;
  }
  if (globe_surface_loaded_ && globe_pass_.has_surface()) {
    if (atmosphere_) {
      const float az = atmosphere_->params().sun_azimuth_rad;
      const float el = atmosphere_->params().sun_elevation_rad;
      const float cos_el = std::cos(el);
      globe_pass_.set_sun_direction(std::cos(az) * cos_el, std::sin(el),
                                    std::sin(az) * cos_el);
    }
    return true;
  }

  std::string path;
  std::string imagery_path;
  double minx = 73.0;
  double miny = 18.0;
  double maxx = 135.0;
  double maxy = 54.0;
  std::vector<float> heights;
  std::vector<uint8_t> rgba;
  int cols = 0;
  int rows = 0;
  int tw = 0;
  int th = 0;
  bool dem_looks_global = false;
  // DemRaster (GDAL) dies before set_dem_surface. Sampling and the
  // hypsometric fallback live on DemRaster::sample_globe_surface.
  {
    vista::DemRaster dem;
    // Globe prefers global_dem; fall back to china_dem. No synthetic stand-in.
    path = vista::find_sample_global_dem_path();
    bool loaded = false;
    if (!path.empty()) {
      loaded = dem.load_gdal_raster(path.c_str()) && !dem.empty();
    }
    if (!loaded) {
      path = vista::find_sample_dem_path();
      loaded = !path.empty() && dem.load_gdal_raster(path.c_str()) && !dem.empty();
    }
    if (!loaded) {
      return false;
    }
    dem.fit_vertical_exaggeration();

    dem.envelope(&minx, &miny, &maxx, &maxy);
    if (maxx <= minx || maxy <= miny) {
      minx = 73.0;
      miny = 18.0;
      maxx = 135.0;
      maxy = 54.0;
    }

    // Prefer full-sphere sampling when DEM spans the world (global_dem.tif).
    // Regional china_dem keeps a soft-edged window on the ocean sphere.
    dem_looks_global =
        (minx <= -170.0 && maxx >= 170.0 && miny <= -80.0 && maxy >= 80.0);
    if (dem_looks_global) {
      minx = -180.0;
      miny = -90.0;
      maxx = 180.0;
      maxy = 90.0;
    }

    // Terrain / satellite equirect only when DEM is global �?otherwise a
    // full-earth PNG would be UV-mapped into the China window incorrectly.
    imagery_path = dem_looks_global ? vista::find_sample_global_imagery_path()
                                    : vista::find_sample_imagery_path();
    if (!imagery_path.empty()) {
      const bool img_is_global =
          imagery_path.find("global_terrain") != std::string::npos ||
          imagery_path.find("global_imagery") != std::string::npos ||
          imagery_path.find("blue_marble") != std::string::npos;
      if (img_is_global && !dem_looks_global) {
        imagery_path.clear();
      }
    }
    bool imagery_loaded = false;
    if (!dem.sample_globe_surface(
            minx, miny, maxx, maxy, dem_looks_global,
            imagery_path.empty() ? nullptr : imagery_path.c_str(), &heights,
            &cols, &rows, &rgba, &tw, &th, &imagery_loaded)) {
      heights.clear();
    }
    if (!imagery_loaded) {
      imagery_path.clear();
    }
  }
  globe_pass_.set_dem_surface(minx, miny, maxx, maxy, cols, rows,
                              heights.empty() ? nullptr : heights.data(),
                              heights.size(),
                              rgba.empty() ? nullptr : rgba.data(), tw, th);
  {
    vista::GlobeDrawParams gp = globe_pass_.params();
    // Amplify Earth radius so DEM skim clearance/R is small (near-flat).
    // Space orbit parks at ~3.5 R (above china_detail load) for a readable
    // full-sphere ocean-blue capture.
    gp.radius = 2.0f;
    gp.height_scale = 1.8e-6f;
    gp.ambient = 0.48f;
    gp.intensity = 0.38f;
    // Near-earth flythrough / China window: fine UV sphere for DEM slope.
    gp.lon_slices = dem_looks_global ? 384 : 320;
    gp.lat_slices = dem_looks_global ? 192 : 160;
    globe_pass_.set_params(gp);
  }
  if (dem_looks_global) {
    vista::SatCloudDrawParams sc = sat_cloud_pass_.params();
    // Sit just above mild DEM peaks on the sphere.
    sc.shell_radius = 2.0f * 1.03f;
    sc.lon_slices = 128;
    sc.lat_slices = 64;
    sc.opacity = 0.38f;
    sc.soft_edge = 0.22f;
    sat_cloud_pass_.set_params(sc);
  }
  if (atmosphere_) {
    const float az = atmosphere_->params().sun_azimuth_rad;
    const float el = atmosphere_->params().sun_elevation_rad;
    const float cos_el = std::cos(el);
    globe_pass_.set_sun_direction(std::cos(az) * cos_el, std::sin(el),
                                  std::sin(az) * cos_el);
  }
  globe_surface_loaded_ = globe_pass_.has_surface();
  if (globe_surface_loaded_) {
    std::fprintf(stderr,
                 "atmosphere.globe: dem=%s global=%d envelope=[%.1f,%.1f]-"
                 "[%.1f,%.1f] albedo=%s\n",
                 path.empty() ? "(synthetic)" : path.c_str(),
                 globe_pass_.dem_is_global() ? 1 : 0, minx, miny, maxx, maxy,
                 imagery_path.empty() ? "(hypsometric)" : imagery_path.c_str());
    std::fflush(stderr);
  }
  return globe_surface_loaded_;
}

bool AtmosphereSession::prepare_sat_clouds() {
  atmosphere_frame_.set_sat_cloud_enabled(sat_cloud_enabled_);
  if (!sat_cloud_enabled_) {
    return true;
  }
  sat_cloud_pass_.set_time_sec(time_sec());
  if (sat_cloud_cover_loaded_ && sat_cloud_pass_.has_cover()) {
    return true;
  }
  std::fprintf(stderr, "atmosphere.sat_cloud: prepare begin\n");
  std::fflush(stderr);
  const std::string path = find_sample_sat_cloud_path();
  if (!path.empty()) {
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
    if (vista::load_imagery_rgba(path.c_str(), &rgba, &w, &h) && w > 0 && h > 0) {
      sat_cloud_pass_.set_cover_rgba(rgba.data(), w, h);
      sat_cloud_cover_loaded_ = sat_cloud_pass_.has_cover();
      std::fprintf(stderr, "atmosphere.sat_cloud: loaded %s %dx%d\n",
                   path.c_str(), w, h);
      std::fflush(stderr);
      return sat_cloud_cover_loaded_;
    }
  }
  sat_cloud_pass_.seed_procedural_cover(256, 128, 7);
  sat_cloud_cover_loaded_ = sat_cloud_pass_.has_cover();
  std::fprintf(stderr,
               "atmosphere.sat_cloud: procedural stub (drop sat_cloud.tif "
               "under out/data)\n");
  std::fflush(stderr);
  return sat_cloud_cover_loaded_;
}

}  // namespace content
