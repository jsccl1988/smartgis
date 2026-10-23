// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Web Mercator XYZ quadtree (slippy map / ol.source.XYZ index). Lon/lat in
// degrees, zoom in [0, 22], tile origin at the northwest corner.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_WEB_MERCATOR_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_WEB_MERCATOR_H_

#include <string>
#include <string_view>
#include <vector>

namespace content {
namespace tile {

struct TileIndex {
  int z = 0;
  int x = 0;
  int y = 0;
};

// Geographic bounds. South/north stay inside the Web Mercator latitude limit.
struct LonLatBox {
  double west = 0;
  double south = 0;
  double east = 0;
  double north = 0;
};

inline constexpr int k_max_zoom = 22;
inline constexpr double k_max_mercator_lat = 85.0511287798066;
inline constexpr double k_earth_radius_m = 6378137.0;

int clamp_zoom(int z);

// Floor index of the tile that contains |lon|/|lat| at |z|.
TileIndex lonlat_to_tile(double lon_deg, double lat_deg, int z);

// Geographic bbox of one XYZ tile. Y grows south.
LonLatBox tile_lonlat_bbox(int z, int x, int y);

// Ground meters per pixel at |lat_deg| for a |tile_px| tile at |z|.
double resolution_m_per_px(int z, double lat_deg, int tile_px = 256);

// Zoom whose tiles are about |tile_px| wide across |width_px| of |span_lon_deg|.
int zoom_for_span(double span_lon_deg, int width_px, int tile_px = 256);

// Tiles overlapping |box| at |z| (or zoom_for_span when |z| < 0). Capped.
std::vector<TileIndex> tiles_for_extent(const LonLatBox& box, int z,
                                        int width_px, int max_tiles = 64);

// {z} {x} {y} and optional {s}->a, {-y} TMS row.
std::string format_xyz_url(std::string_view tmpl, int z, int x, int y);

}  // namespace tile
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_WEB_MERCATOR_H_
