// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include "content/browser/document/map_scene.h"
#include "gis/vista/domain/atmosphere/systems/atmosphere_params.h"
#include "gis/vista/domain/atmosphere/systems/cloud_system.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "gis/vista/domain/atmosphere/field/field_ingest.h"
#include "gis/vista/domain/atmosphere/systems/ocean_system.h"
#include "gis/vista/assets/tileset/tileset.h"
#include "gis/vista/world/terrain/dem_frame.h"
#include "gis/vista/world/terrain/dem_raster.h"
#include "gis/vista/world/world.h"
#include "effect/atmosphere/cloud/cloud_pass.h"
#include "effect/atmosphere/fog/fog_pass.h"
#include "effect/atmosphere/ocean/ocean_pass.h"
#include "effect/atmosphere/sky/sky_pass.h"
#include "base/trace/event/process_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
}

AtmosphereSession::~AtmosphereSession() {
  release_passes();
}

void AtmosphereSession::bind_scene(const MapScene* scene) {
  scene_ = scene;
}

void AtmosphereSession::bind_gpu(Scene3dGpuPresent* gpu) {
  gpu_ = gpu;
}

Extent2 AtmosphereSession::world_extent() const {
  if (gpu_) {
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
  return prepare_ocean() && prepare_clouds() && prepare_sky() && prepare_fog();
}

void AtmosphereSession::advance_sim_time() {
  if (!atmosphere_) {
    return;
  }
  // Ocean FFT / cloud cover scrub need a moving clock; sky/fog are static
  // without it and look "frozen" in interactive present.
  if (!atmosphere_->ocean_enabled() && !atmosphere_->cloud_enabled() &&
      !atmosphere_->sky_enabled()) {
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
  gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  const double t = atmosphere_->time_sec();
  constexpr double kTwoPi = 6.283185307179586;
  p.sun_azimuth_rad =
      static_cast<float>(std::fmod(0.55 + t * 0.0523598775598, kTwoPi));
  p.sun_elevation_rad =
      static_cast<float>(0.38 + 0.28 * std::sin(t * 0.041));
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
}

gis::atmosphere::Environment& AtmosphereSession::ensure() {
  if (!atmosphere_) {
    atmosphere_ = std::make_unique<gis::atmosphere::Environment>();
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

void AtmosphereSession::set_wind_overlay_enabled(bool on) {
  wind_overlay_enabled_ = on;
  if (on) {
    // Need WindU/V samples for arrows; seed procedural if store empty.
    gis::atmosphere::Environment& env = ensure();
    if (env.field_store().layer_count() == 0) {
      seed_procedural();
    }
  }
}

void AtmosphereSession::set_time_sec(double t) {
  gis::atmosphere::Environment& env = ensure();
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

double AtmosphereSession::time_sec() const {
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

bool AtmosphereSession::load_fields(std::string_view spec) {
  // Parse path[:channel[:time]][,...] then batch by channel into
  // Environment::load_external_series (ingest_gdal_field_series).
  spec = trim_ascii(spec);
  if (spec.empty()) {
    return false;
  }
  gis::atmosphere::Environment& env = ensure();

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

gis::atmosphere::FieldGrid AtmosphereSession::field_grid() const {
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

void AtmosphereSession::seed_procedural() {
  seed_procedural(true);
}

void AtmosphereSession::seed_procedural(bool with_land_rings) {
  gis::atmosphere::Environment& env = ensure();
  std::vector<gis::LonLatRing> rings;
  if (with_land_rings && scene_) {
    scene_->export_land_rings(&rings);
  }
  env.seed_procedural_baseline(field_grid(),
                               rings.empty() ? nullptr : &rings);
  env.sync_systems_from_params();
}

void AtmosphereSession::enable_demo() {
  gis::atmosphere::Environment& env = ensure();
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  env.enable_demo(field_grid(),
                  rings.empty() ? nullptr : &rings);
}

namespace {

bool m3_resolve_stub(const char* uri, gis::ModelAsset* out, size_t* byte_cost,
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
    gis::World dem_world;
    gis::Node* dem =
        gis::seed_china_dem_into_world(&dem_world, nullptr, 0, "m3_dem", 48);
    if (!dem || !dem->has_terrain_mesh()) {
      return m3_fail(err, "m3-dem-ok");
    }
    bool height_signal = false;
    const std::vector<float>& pos = dem->terrain_positions;
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
    gis::Tileset tileset;
    if (!gis::parse_tileset_json(ts_json, std::strlen(ts_json), tileset)) {
      return m3_fail(err, "m3-tiles-ok");
    }
    gis::World world;
    gis::Node* node = world.attach_tileset(&tileset, "m3_city");
    if (!node) {
      return m3_fail(err, "m3-tiles-ok");
    }
    gis::ViewState view;
    view.eye_x = 2.05;
    view.eye_y = 0.65;
    view.eye_z = 0.05;
    view.sse_denominator = 1;
    if (!world.stream_tileset(node->id, view, 0, 8)) {
      return m3_fail(err, "m3-tiles-ok");
    }
    const gis::Node* streamed = world.find(node->id);
    if (!streamed || streamed->visible_uris.empty()) {
      return m3_fail(err, "m3-tiles-ok");
    }

    gis::TilesetContentCache cache(600);
    std::vector<const gis::Tile*> visible;
    gis::select_tiles(tileset, view, 0, visible);
    gis::ensure_tileset_content(visible, &cache, m3_resolve_stub, nullptr);
    if (cache.resident_bytes() > cache.max_bytes()) {
      return m3_fail(err, "m3-tiles-ok");
    }
    if (!cache.contains("missing.glb")) {
      return m3_fail(err, "m3-tiles-ok");
    }
    const gis::TilesetContentEntry* miss = cache.try_get("missing.glb");
    if (!miss || miss->decode_ok) {
      return m3_fail(err, "m3-tiles-ok");
    }
    // Second put of another leaf must stay under budget (LRU eviction).
    gis::ModelAsset extra;
    extra.name = "extra";
    if (!cache.put("city_a.glb", extra, 512, true) &&
        cache.resident_bytes() > cache.max_bytes()) {
      return m3_fail(err, "m3-tiles-ok");
    }
    if (cache.resident_bytes() > cache.max_bytes()) {
      return m3_fail(err, "m3-tiles-ok");
    }
  }

  // --- m3-atmosphere-ok: demo on (ocean/cloud/sky/fog), then all off.
  {
    enable_demo();
    const gis::atmosphere::Environment* env = environment();
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

  effect::atmosphere::OceanDrawParams draw;
  // Wave height: GIS meters → orbit Y (same scale as DEM elev).
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
  // China orbit span ≈ 3.2: keep a readable lip without swallowing DEM peaks.
  // Prefer Gerstner for interactive full-China (GPU FFT can look flat when the
  // height-map energy is tiny after 1/N² at this scale).
  draw.significant_wave_height =
      (std::max)((std::min)(draw.significant_wave_height * 4.5f, 0.22f), 0.10f);
  draw.use_gerstner_fallback = true;
  draw.chop = (std::max)(draw.chop, 1.15f);
  draw.shininess = (std::max)(draw.shininess, 180.0f);
  draw.mesh_resolution = 65;
  // Mid-tier GIS water: deep navy, muted shelf — never near-cyan albedo.
  draw.deep_r = 0.02f;
  draw.deep_g = 0.07f;
  draw.deep_b = 0.18f;
  draw.shallow_r = 0.06f;
  draw.shallow_g = 0.22f;
  draw.shallow_b = 0.32f;
  draw.fresnel_bias = 0.03f;
  draw.fresnel_power = 6.0f;
  ocean_pass_.set_params(draw);
  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  ocean_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                             p.sun_elevation_rad);
  ocean_pass_.set_time_sec(atmosphere_->time_sec());

  constexpr int kMask = 32;
  std::vector<float> mask(
      static_cast<std::size_t>(kMask) * static_cast<std::size_t>(kMask), 1.f);
  extent.cols = kMask;
  extent.rows = kMask;
  if (!atmosphere_->ocean_system().fill_sea_mask_grid(
          atmosphere_->field_store(), extent, kMask, kMask,
          atmosphere_->time_sec(), mask.data(), mask.size())) {
    // Fail closed: no sea_mask layer means do not cover the DEM with a
    // full-screen ocean (mask=1). Interactive 3D used to open blank cyan
    // when seed_procedural was skipped (has_china_extent false).
    std::fill(mask.begin(), mask.end(), 0.f);
  }
  ocean_pass_.set_sea_mask_cpu(kMask, kMask, mask.data(), mask.size());
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
  const gis::atmosphere::CloudSample sample =
      atmosphere_->cloud_system().sample_at(
          atmosphere_->field_store(), clon, clat, atmosphere_->time_sec());

  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  cloud_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                             p.sun_elevation_rad);
  cloud_pass_.set_cloud_slab(sample.base_m, sample.top_m);
  // Mid-tier broken deck: readable cover without bleaching DEM greens.
  cloud_pass_.set_cover_modulation(
      (std::max)(0.28f, (std::min)(sample.cover, 0.55f)));

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
  lift = (std::max)(0.18f, (std::min)(lift, 0.55f));
  float thick = gpu_->geo_frame().meters_to_orbit_y(sample.top_m - sample.base_m);
  thick = (std::max)(0.12f, (std::min)(thick, 0.30f));
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
  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  sky_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                           p.sun_elevation_rad);
  effect::atmosphere::SkyDrawParams sky = sky_pass_.params();
  // Past max orbit distance (12) so zoom-out stays inside the sky.
  sky.dome_radius = 40.0f;
  // Industry mid-tier analytical dome: deep Rayleigh zenith, warmer haze
  // horizon, readable sun disk/corona (Bruneton-lite, no LUT).
  sky.zenith_r = 0.06f;
  sky.zenith_g = 0.20f;
  sky.zenith_b = 0.78f;
  sky.horizon_r = 0.62f;
  sky.horizon_g = 0.74f;
  sky.horizon_b = 0.88f;
  sky.sunset_r = 0.92f;
  sky.sunset_g = 0.48f;
  sky.sunset_b = 0.28f;
  sky.sun_glow_strength = 0.55f;
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
  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  effect::atmosphere::FogDrawParams fog;
  // Soft aerial haze on terrain only (sky depth is skipped in FogPass HLSL).
  // Cap opacity so hypsometric greens still pass showcase landish gates.
  fog.density = (std::max)((std::min)(p.fog_density, 0.12f), 0.05f);
  // China orbit frame span ~3.2: keep haze visible without washing terrain.
  fog.visibility = (std::max)(2.4f, (std::min)(p.fog_visibility, 4.5f));
  fog.height_falloff = p.fog_height_falloff;
  fog.max_opacity = (std::min)((std::max)(p.fog_max_opacity, 0.14f), 0.24f);
  fog.base_height = gpu_->geo_frame().sea_level_y();
  // Tint haze toward the analytical sky horizon (matches sky pass).
  // Zero sun_glow for the tint sample — a fixed horizon ray can align with
  // the sun and pick up disk/corona, blowing fog to near-white and washing
  // the showcase BMP (blue_sky / landish gates fail).
  float hr = fog.color_r;
  float hg = fog.color_g;
  float hb = fog.color_b;
  effect::atmosphere::SkyDrawParams tint = sky_pass_.params();
  tint.sun_glow_strength = 0.f;
  effect::atmosphere::SkyPass::sample_sky_rgb(tint, 0.f, 0.05f, 1.f, &hr, &hg,
                                              &hb);
  fog.color_r = hr;
  fog.color_g = hg;
  fog.color_b = hb;
  fog_pass_.set_params(fog);
  return true;
}


}  // namespace content
