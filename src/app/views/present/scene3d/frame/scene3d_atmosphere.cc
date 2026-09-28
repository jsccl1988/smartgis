// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/scene3d/scene3d_presenter.h"

#include "app/views/document/map_scene.h"
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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace app {

void Scene3dPresenter::release_atmosphere_passes() {
  ocean_pass_.release();
  cloud_pass_.release();
  sky_pass_.release();
  fog_pass_.release();
}

gis::atmosphere::Environment& Scene3dPresenter::ensure_atmosphere() {
  if (!atmosphere_) {
    atmosphere_ = std::make_unique<gis::atmosphere::Environment>();
  }
  return *atmosphere_;
}

void Scene3dPresenter::set_ocean_enabled(bool on) {
  ensure_atmosphere().set_ocean_enabled(on);
}

void Scene3dPresenter::set_cloud_enabled(bool on) {
  ensure_atmosphere().set_cloud_enabled(on);
}

void Scene3dPresenter::set_sky_enabled(bool on) {
  ensure_atmosphere().set_sky_enabled(on);
}

void Scene3dPresenter::set_fog_enabled(bool on) {
  ensure_atmosphere().set_fog_enabled(on);
}

void Scene3dPresenter::set_wind_overlay_enabled(bool on) {
  wind_overlay_enabled_ = on;
  if (on) {
    // Need WindU/V samples for arrows; seed procedural if store empty.
    gis::atmosphere::Environment& env = ensure_atmosphere();
    if (env.field_store().layer_count() == 0) {
      seed_atmosphere_procedural();
    }
  }
}

void Scene3dPresenter::set_time_sec(double t) {
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

double Scene3dPresenter::time_sec() const {
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

bool Scene3dPresenter::load_atmosphere_fields(std::string_view spec) {
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

gis::atmosphere::FieldGrid Scene3dPresenter::atmosphere_field_grid() const {
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

void Scene3dPresenter::seed_atmosphere_procedural() {
  gis::atmosphere::Environment& env = ensure_atmosphere();
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  env.seed_procedural_baseline(atmosphere_field_grid(),
                               rings.empty() ? nullptr : &rings);
  env.sync_systems_from_params();
}

void Scene3dPresenter::enable_atmosphere_demo() {
  gis::atmosphere::Environment& env = ensure_atmosphere();
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  env.enable_demo(atmosphere_field_grid(),
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

bool Scene3dPresenter::run_m3_self_test_hooks(std::string* err) {
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
    enable_atmosphere_demo();
    const gis::atmosphere::Environment* env = atmosphere();
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


bool Scene3dPresenter::prepare_atmosphere_ocean() {
  if (!atmosphere_ || !atmosphere_->ocean_enabled()) {
    return true;
  }
  if (!geo_frame_.valid) {
    geo_frame_ = OrbitGeoFrame::from_extent(world_extent());
  }
  atmosphere_->sync_systems_from_params();

  const content::Extent2 e = geo_frame_.extent;
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
  geo_frame_.extent_orbit_xz(&min_x, &max_x, &min_z, &max_z);
  const float pad = OrbitGeoFrame::kOceanPad;
  min_x -= pad;
  max_x += pad;
  min_z -= pad;
  max_z += pad;

  effect::atmosphere::OceanDrawParams draw;
  // Wave height: GIS meters → orbit Y (same scale as DEM elev).
  draw.significant_wave_height = (std::max)(
      0.01f, geo_frame_.meters_to_orbit_y(tile.spectrum.significant_wave_height));
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
  draw.patch_y = geo_frame_.sea_level_y();
  draw.patch_half_x = 0.5f * (max_x - min_x);
  draw.patch_half_z = 0.5f * (max_z - min_z);
  draw.patch_half_extent =
      (std::max)(draw.patch_half_x, draw.patch_half_z);
  draw.mesh_resolution = 33;
  ocean_pass_.set_params(draw);
  ocean_pass_.set_time_sec(atmosphere_->time_sec());

  constexpr int kMask = 32;
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
  return true;
}

bool Scene3dPresenter::prepare_atmosphere_clouds() {
  if (!atmosphere_ || !atmosphere_->cloud_enabled()) {
    return true;
  }
  if (!geo_frame_.valid) {
    geo_frame_ = OrbitGeoFrame::from_extent(world_extent());
  }
  const content::Extent2 e = geo_frame_.extent;
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

  float min_x = 0.f;
  float max_x = 0.f;
  float min_z = 0.f;
  float max_z = 0.f;
  geo_frame_.extent_orbit_xz(&min_x, &max_x, &min_z, &max_z);
  const float half_x = 0.5f * (max_x - min_x) + OrbitGeoFrame::kOceanPad;
  const float half_z = 0.5f * (max_z - min_z) + OrbitGeoFrame::kOceanPad;
  const float base_y =
      geo_frame_.sea_level_y() + geo_frame_.meters_to_orbit_y(sample.base_m);
  const float top_y =
      geo_frame_.sea_level_y() + geo_frame_.meters_to_orbit_y(sample.top_m);
  const float deck_y = 0.5f * (base_y + top_y);
  cloud_pass_.set_deck_orbit(half_x, half_z, deck_y);
  cloud_pass_.set_slab_orbit(base_y, top_y);
  return true;
}

bool Scene3dPresenter::prepare_atmosphere_sky() {
  if (!atmosphere_ || !atmosphere_->sky_enabled()) {
    return true;
  }
  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  sky_pass_.set_sun_from_azimuth_elevation(p.sun_azimuth_rad,
                                           p.sun_elevation_rad);
  effect::atmosphere::SkyDrawParams sky = sky_pass_.params();
  // Dome covers the China orbit frame (span ~3.2 + pad).
  sky.dome_radius = 8.0f;
  sky_pass_.set_params(sky);
  return true;
}

bool Scene3dPresenter::prepare_atmosphere_fog() {
  if (!atmosphere_ || !atmosphere_->fog_enabled()) {
    return true;
  }
  if (!geo_frame_.valid) {
    geo_frame_ = OrbitGeoFrame::from_extent(world_extent());
  }
  const gis::atmosphere::AtmosphereParams& p = atmosphere_->params();
  effect::atmosphere::FogDrawParams fog;
  fog.density = p.fog_density;
  // Visibility is already in orbit-ish units in AtmosphereParams defaults;
  // clamp so the China frame (~3.2 span) still shows terrain through haze.
  fog.visibility = (std::max)(2.0f, p.fog_visibility);
  fog.height_falloff = p.fog_height_falloff;
  fog.max_opacity = p.fog_max_opacity;
  fog.base_height = geo_frame_.sea_level_y();
  fog_pass_.set_params(fog);
  return true;
}


}  // namespace app
