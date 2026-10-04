// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/raster/tile/mosaic.h"

#include "gis/envelope.h"
#include "gpu/raster/tile/decode.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gpu {
namespace detail {
namespace {

// Map tile world rect into output pixels and overwrite |dst| (same layer).
void blit_tile_world(std::vector<uint8_t>* dst, uint32_t dw, uint32_t dh,
                     const content::Extent2& extent, const gis::Envelope& world,
                     const std::vector<uint8_t>& tile, uint32_t tw,
                     uint32_t th) {
  if (!dst || dw == 0 || dh == 0 || tw == 0 || th == 0 ||
      tile.size() < static_cast<size_t>(tw) * th * 4u) {
    return;
  }
  const double ex0 = extent.xmin;
  const double ey0 = extent.ymin;
  const double ex1 = extent.xmax;
  const double ey1 = extent.ymax;
  const double span_x = ex1 - ex0;
  const double span_y = ey1 - ey0;
  if (span_x <= 0.0 || span_y <= 0.0) {
    return;
  }
  const double tx0 = world.MinX;
  const double ty0 = world.MinY;
  const double tx1 = world.MaxX;
  const double ty1 = world.MaxY;
  if (!(tx1 > tx0 && ty1 > ty0)) {
    return;
  }

  const double px0 = (tx0 - ex0) / span_x * static_cast<double>(dw);
  const double px1 = (tx1 - ex0) / span_x * static_cast<double>(dw);
  const double py0 = (ey1 - ty1) / span_y * static_cast<double>(dh);
  const double py1 = (ey1 - ty0) / span_y * static_cast<double>(dh);

  const int ix0 = (std::max)(0, static_cast<int>(std::floor(px0)));
  const int iy0 = (std::max)(0, static_cast<int>(std::floor(py0)));
  const int ix1 =
      (std::min)(static_cast<int>(dw), static_cast<int>(std::ceil(px1)));
  const int iy1 =
      (std::min)(static_cast<int>(dh), static_cast<int>(std::ceil(py1)));

  for (int y = iy0; y < iy1; ++y) {
    for (int x = ix0; x < ix1; ++x) {
      const double wx =
          ex0 + (static_cast<double>(x) + 0.5) / static_cast<double>(dw) *
                    span_x;
      const double wy =
          ey1 - (static_cast<double>(y) + 0.5) / static_cast<double>(dh) *
                    span_y;
      if (wx < tx0 || wx >= tx1 || wy < ty0 || wy >= ty1) {
        continue;
      }
      const double u = (wx - tx0) / (tx1 - tx0);
      const double v = (ty1 - wy) / (ty1 - ty0);
      int sx = static_cast<int>(u * static_cast<double>(tw));
      int sy = static_cast<int>(v * static_cast<double>(th));
      if (sx < 0) {
        sx = 0;
      }
      if (sx >= static_cast<int>(tw)) {
        sx = static_cast<int>(tw) - 1;
      }
      if (sy < 0) {
        sy = 0;
      }
      if (sy >= static_cast<int>(th)) {
        sy = static_cast<int>(th) - 1;
      }
      const size_t di =
          (static_cast<size_t>(y) * dw + static_cast<size_t>(x)) * 4u;
      const size_t si =
          (static_cast<size_t>(sy) * tw + static_cast<size_t>(sx)) * 4u;
      (*dst)[di + 0] = tile[si + 0];
      (*dst)[di + 1] = tile[si + 1];
      (*dst)[di + 2] = tile[si + 2];
      (*dst)[di + 3] = tile[si + 3];
    }
  }
}

}  // namespace

bool extent_is_valid(const content::Extent2& e) {
  return e.xmax > e.xmin && e.ymax > e.ymin;
}

gis::tile::Viewport viewport_from_request(const DrawRequest& req) {
  gis::tile::Viewport vp;
  vp.min_x = req.extent.xmin;
  vp.min_y = req.extent.ymin;
  vp.max_x = req.extent.xmax;
  vp.max_y = req.extent.ymax;
  vp.z = req.zoom;
  return vp;
}

std::vector<gis::tile::TileCoord> visible_tile_coords(const DrawRequest& req) {
  if (!extent_is_valid(req.extent)) {
    return {gis::tile::TileCoord{0, 0, 0}};
  }
  return gis::tile::tiles_for_viewport(viewport_from_request(req));
}

bool decode_tile_bgra(const std::string& bytes, uint32_t dst_w, uint32_t dst_h,
                      std::vector<uint8_t>* dst) {
  std::vector<uint8_t> tile;
  uint32_t tw = 0;
  uint32_t th = 0;
  if (!decode_tile_native(bytes, &tile, &tw, &th) || tw == 0 || th == 0) {
    return false;
  }
  dst->assign(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u, 0);
  for (uint32_t y = 0; y < dst_h; ++y) {
    const uint32_t sy = (y * th) / dst_h;
    for (uint32_t x = 0; x < dst_w; ++x) {
      const uint32_t sx = (x * tw) / dst_w;
      const size_t di = (static_cast<size_t>(y) * dst_w + x) * 4u;
      const size_t si = (static_cast<size_t>(sy) * tw + sx) * 4u;
      (*dst)[di + 0] = tile[si + 0];
      (*dst)[di + 1] = tile[si + 1];
      (*dst)[di + 2] = tile[si + 2];
      (*dst)[di + 3] = tile[si + 3];
    }
  }
  return true;
}

bool mosaic_tile_bytes(std::vector<uint8_t>* layer, uint32_t w, uint32_t h,
                       const DrawRequest& req, const gis::tile::TileCoord& coord,
                       const std::string& body) {
  if (body.empty()) {
    return false;
  }
  if (!extent_is_valid(req.extent)) {
    return decode_tile_bgra(body, w, h, layer);
  }
  std::vector<uint8_t> tile;
  uint32_t tw = 0;
  uint32_t th = 0;
  if (!decode_tile_native(body, &tile, &tw, &th)) {
    return false;
  }
  const gis::Envelope world =
      gis::tile::tile_world_rect(coord.z, coord.x, coord.y);
  blit_tile_world(layer, w, h, req.extent, world, tile, tw, th);
  return true;
}

}  // namespace detail
}  // namespace gpu
