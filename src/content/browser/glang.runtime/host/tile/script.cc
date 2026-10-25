// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/tile/verbs.h"

#include "base/core/log.h"
#include "gis/tile/protocol/lonlat_xyz.h"
#include "gis/tile/provider/xyz_source.h"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace content {
namespace tile {
namespace {

gis::tile::XyzSource& script_source() {
  static gis::tile::XyzSource source;
  return source;
}

bool parse_i32(std::string_view text, int* out) {
  if (!out || text.empty()) {
    return false;
  }
  int value = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end) {
    return false;
  }
  *out = value;
  return true;
}

bool parse_f64(std::string_view text, double* out) {
  if (!out || text.empty()) {
    return false;
  }
  const std::string copy(text);
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str()) {
    return false;
  }
  *out = value;
  return true;
}

const std::string* arg_at(const content::code::ScriptCall& call, int index) {
  if (index < 0 || index >= static_cast<int>(call.positional.size())) {
    return nullptr;
  }
  return &call.positional[static_cast<std::size_t>(index)];
}

void set_error(std::string* error, const char* why) {
  if (error) {
    *error = why ? why : "tile verb failed";
  }
  LOGGING(LOG_ERROR, "tile verb: %s", why ? why : "failed");
}

std::string format_box(const gis::tile::LonLatBox& box) {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%.10g,%.10g,%.10g,%.10g", box.west, box.south,
                box.east, box.north);
  return buf;
}

bool is_tile_verb(std::string_view name) {
  return name == "tile_xyz" || name == "tile_bbox" || name == "tile_resolution" ||
         name == "tile_url" || name == "tile_fetch";
}

}  // namespace

bool try_script(const content::code::ScriptCall& call, std::string* result,
                std::string* error) {
  if (!is_tile_verb(call.name)) {
    return false;
  }
  if (call.name == "tile_xyz") {
    const std::string* lon_s = arg_at(call, 0);
    const std::string* lat_s = arg_at(call, 1);
    const std::string* z_s = arg_at(call, 2);
    double lon = 0;
    double lat = 0;
    int z = 0;
    if (!lon_s || !lat_s || !z_s || !parse_f64(*lon_s, &lon) ||
        !parse_f64(*lat_s, &lat) || !parse_i32(*z_s, &z)) {
      set_error(error, "tile_xyz needs lon lat z");
      return true;
    }
    const gis::tile::TileIndex tile = gis::tile::lonlat_to_tile(lon, lat, z);
    if (result) {
      *result = std::to_string(tile.z) + "," + std::to_string(tile.x) + "," +
                std::to_string(tile.y);
    }
    return true;
  }
  if (call.name == "tile_bbox") {
    const std::string* z_s = arg_at(call, 0);
    const std::string* x_s = arg_at(call, 1);
    const std::string* y_s = arg_at(call, 2);
    int z = 0;
    int x = 0;
    int y = 0;
    if (!z_s || !x_s || !y_s || !parse_i32(*z_s, &z) || !parse_i32(*x_s, &x) ||
        !parse_i32(*y_s, &y)) {
      set_error(error, "tile_bbox needs z x y");
      return true;
    }
    if (result) {
      *result = format_box(gis::tile::tile_lonlat_bbox(z, x, y));
    }
    return true;
  }
  if (call.name == "tile_resolution") {
    const std::string* z_s = arg_at(call, 0);
    const std::string* lat_s = arg_at(call, 1);
    int z = 0;
    double lat = 0;
    if (!z_s || !lat_s || !parse_i32(*z_s, &z) || !parse_f64(*lat_s, &lat)) {
      set_error(error, "tile_resolution needs z lat");
      return true;
    }
    if (result) {
      *result = std::to_string(gis::tile::resolution_m_per_px(z, lat, 256));
    }
    return true;
  }
  if (call.name == "tile_url") {
    const std::string* tmpl = arg_at(call, 0);
    const std::string* z_s = arg_at(call, 1);
    const std::string* x_s = arg_at(call, 2);
    const std::string* y_s = arg_at(call, 3);
    int z = 0;
    int x = 0;
    int y = 0;
    if (!tmpl || !z_s || !x_s || !y_s || !parse_i32(*z_s, &z) ||
        !parse_i32(*x_s, &x) || !parse_i32(*y_s, &y)) {
      set_error(error, "tile_url needs template z x y");
      return true;
    }
    if (result) {
      *result = gis::tile::format_xyz_url(*tmpl, z, x, y);
    }
    return true;
  }
  const std::string* tmpl = arg_at(call, 0);
  const std::string* z_s = arg_at(call, 1);
  const std::string* x_s = arg_at(call, 2);
  const std::string* y_s = arg_at(call, 3);
  int z = 0;
  int x = 0;
  int y = 0;
  if (!tmpl || !z_s || !x_s || !y_s || !parse_i32(*z_s, &z) ||
      !parse_i32(*x_s, &x) || !parse_i32(*y_s, &y)) {
    set_error(error, "tile_fetch needs template z x y");
    return true;
  }
  script_source().set_url_template(*tmpl);
  std::string body;
  std::string fetch_error;
  if (!script_source().get(z, x, y, &body, &fetch_error)) {
    set_error(error, fetch_error.empty() ? "tile_fetch failed" : fetch_error.c_str());
    return true;
  }
  if (result) {
    *result = "bytes=" + std::to_string(body.size());
  }
  return true;
}

}  // namespace tile
}  // namespace content