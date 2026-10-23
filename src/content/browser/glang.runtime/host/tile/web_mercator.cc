// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/tile/web_mercator.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace content {
namespace tile {
namespace {

constexpr double k_pi = 3.14159265358979323846;

double clamp_lat(double lat_deg) {
  return std::clamp(lat_deg, -k_max_mercator_lat, k_max_mercator_lat);
}

int clamp_index(int index, int n) {
  if (n <= 0) {
    return 0;
  }
  if (index < 0) {
    return 0;
  }
  if (index >= n) {
    return n - 1;
  }
  return index;
}

double mercator_lat_deg(double y_norm) {
  const double merc = k_pi * (1.0 - 2.0 * y_norm);
  return std::atan(std::sinh(merc)) * 180.0 / k_pi;
}

void replace_all(std::string* out, std::string_view from, std::string_view to) {
  if (!out || from.empty()) {
    return;
  }
  std::string::size_type pos = 0;
  while ((pos = out->find(from, pos)) != std::string::npos) {
    out->replace(pos, from.size(), to);
    pos += to.size();
  }
}

}  // namespace

int clamp_zoom(int z) {
  return std::clamp(z, 0, k_max_zoom);
}

TileIndex lonlat_to_tile(double lon_deg, double lat_deg, int z) {
  z = clamp_zoom(z);
  const int n = 1 << z;
  const double lon = std::clamp(lon_deg, -180.0, 180.0);
  const double lat = clamp_lat(lat_deg);
  const double lat_rad = lat * k_pi / 180.0;
  const double x = (lon + 180.0) / 360.0 * static_cast<double>(n);
  const double y =
      (1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / k_pi) /
      2.0 * static_cast<double>(n);
  TileIndex tile;
  tile.z = z;
  tile.x = clamp_index(static_cast<int>(std::floor(x)), n);
  tile.y = clamp_index(static_cast<int>(std::floor(y)), n);
  return tile;
}

LonLatBox tile_lonlat_bbox(int z, int x, int y) {
  z = clamp_zoom(z);
  const int n = 1 << z;
  x = clamp_index(x, n);
  y = clamp_index(y, n);
  const double nf = static_cast<double>(n);
  LonLatBox box;
  box.west = static_cast<double>(x) / nf * 360.0 - 180.0;
  box.east = static_cast<double>(x + 1) / nf * 360.0 - 180.0;
  box.north = mercator_lat_deg(static_cast<double>(y) / nf);
  box.south = mercator_lat_deg(static_cast<double>(y + 1) / nf);
  return box;
}

double resolution_m_per_px(int z, double lat_deg, int tile_px) {
  z = clamp_zoom(z);
  if (tile_px < 1) {
    tile_px = 256;
  }
  const double lat_rad = clamp_lat(lat_deg) * k_pi / 180.0;
  const double circumference = 2.0 * k_pi * k_earth_radius_m;
  const double world_px = static_cast<double>(tile_px) * static_cast<double>(1 << z);
  return std::cos(lat_rad) * circumference / world_px;
}

int zoom_for_span(double span_lon_deg, int width_px, int tile_px) {
  if (!(span_lon_deg > 0.0) || width_px < 1) {
    return 0;
  }
  if (tile_px < 1) {
    tile_px = 256;
  }
  const double tiles_across =
      std::max(1.0, static_cast<double>(width_px) / static_cast<double>(tile_px));
  const double z = std::log2((360.0 * tiles_across) / span_lon_deg);
  return clamp_zoom(static_cast<int>(std::floor(z + 1e-9)));
}

std::vector<TileIndex> tiles_for_extent(const LonLatBox& box, int z,
                                        int width_px, int max_tiles) {
  std::vector<TileIndex> out;
  if (max_tiles < 1) {
    return out;
  }
  const double west = std::clamp(std::min(box.west, box.east), -180.0, 180.0);
  const double east = std::clamp(std::max(box.west, box.east), -180.0, 180.0);
  const double south = clamp_lat(std::min(box.south, box.north));
  const double north = clamp_lat(std::max(box.south, box.north));
  if (z < 0) {
    z = zoom_for_span(east - west, width_px, 256);
  }
  z = clamp_zoom(z);
  const TileIndex nw = lonlat_to_tile(west, north, z);
  const TileIndex se = lonlat_to_tile(std::min(east, 179.999999), south, z);
  const int x0 = std::min(nw.x, se.x);
  const int x1 = std::max(nw.x, se.x);
  const int y0 = std::min(nw.y, se.y);
  const int y1 = std::max(nw.y, se.y);
  out.reserve(static_cast<std::size_t>(max_tiles));
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      out.push_back(TileIndex{z, x, y});
      if (static_cast<int>(out.size()) >= max_tiles) {
        return out;
      }
    }
  }
  return out;
}

std::string format_xyz_url(std::string_view tmpl, int z, int x, int y) {
  z = clamp_zoom(z);
  const int n = 1 << z;
  const int row = n - 1 - clamp_index(y, n);
  std::string out(tmpl);
  replace_all(&out, "{z}", std::to_string(z));
  replace_all(&out, "{x}", std::to_string(clamp_index(x, n)));
  replace_all(&out, "{-y}", std::to_string(row));
  replace_all(&out, "{y}", std::to_string(clamp_index(y, n)));
  replace_all(&out, "{s}", "a");
  return out;
}

}  // namespace tile
}  // namespace content
