// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/present/water_mesh.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/gis_document.h"
#include "plugin/runtime/host/capability/scene3d_sink.h"
#include "vista/terrain/dem/dem_contour.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace plugin {
namespace {

void bake_water_surface_atlas(const double* xyz,
                              int point_count,
                              const float* depth,
                              int dem_width,
                              int dem_height,
                              const double* geotransform,
                              std::vector<uint8_t>* atlas,
                              int atlas_dim,
                              std::vector<float>* uv) {
  atlas->assign(static_cast<size_t>(atlas_dim) * atlas_dim * 4u, 255);
  uv->assign(static_cast<size_t>(point_count) * 2u, 0.f);
  if (!xyz || point_count < 3 || atlas_dim < 8) {
    return;
  }

  double min_lon = xyz[0];
  double max_lon = xyz[0];
  double min_lat = xyz[1];
  double max_lat = xyz[1];
  for (int i = 0; i < point_count; ++i) {
    const double lon = xyz[i * 3];
    const double lat = xyz[i * 3 + 1];
    min_lon = std::min(min_lon, lon);
    max_lon = std::max(max_lon, lon);
    min_lat = std::min(min_lat, lat);
    max_lat = std::max(max_lat, lat);
  }
  const double span_lon = std::max(1.0e-9, max_lon - min_lon);
  const double span_lat = std::max(1.0e-9, max_lat - min_lat);

  for (int i = 0; i < point_count; ++i) {
    const float u = static_cast<float>((xyz[i * 3] - min_lon) / span_lon);
    const float v = static_cast<float>((xyz[i * 3 + 1] - min_lat) / span_lat);
    (*uv)[static_cast<size_t>(i) * 2u] = std::clamp(u, 0.f, 1.f);
    (*uv)[static_cast<size_t>(i) * 2u + 1u] = std::clamp(v, 0.f, 1.f);
  }

  // Cyan free-surface fill with mild banding for color_buckets.
  for (int row = 0; row < atlas_dim; ++row) {
    for (int col = 0; col < atlas_dim; ++col) {
      const size_t p =
          (static_cast<size_t>(row) * atlas_dim + static_cast<size_t>(col)) *
          4u;
      const float t = static_cast<float>(col) /
                      static_cast<float>((std::max)(1, atlas_dim - 1));
      (*atlas)[p] = static_cast<uint8_t>(18 + static_cast<int>(t * 28.f));
      (*atlas)[p + 1] = static_cast<uint8_t>(165 + static_cast<int>(t * 45.f));
      (*atlas)[p + 2] = static_cast<uint8_t>(220 + static_cast<int>(t * 25.f));
      (*atlas)[p + 3] = 0xfa;
    }
  }

  // Surface grid: single-texel soft ink blended into the cyan fill.
  // Opaque near-black @ 128² → ~4.5px bands; soft blend @ 512² targets ~1px.
  // Minors are very light; majors slightly stronger. Step → ~8×8 cells.
  const int grid_step = (std::max)(48, atlas_dim / 8);
  auto stamp_grid_px = [&](int col, int row, bool major) {
    if (col < 0 || row < 0 || col >= atlas_dim || row >= atlas_dim) {
      return;
    }
    const size_t p =
        (static_cast<size_t>(row) * atlas_dim + static_cast<size_t>(col)) * 4u;
    // Keep contrast low so bilinear does not inflate into a dark band.
    const float t = major ? 0.28f : 0.12f;
    const float ink_r = 40.f;
    const float ink_g = 120.f;
    const float ink_b = 160.f;
    (*atlas)[p] = static_cast<uint8_t>(
        (1.f - t) * static_cast<float>((*atlas)[p]) + t * ink_r);
    (*atlas)[p + 1] = static_cast<uint8_t>(
        (1.f - t) * static_cast<float>((*atlas)[p + 1]) + t * ink_g);
    (*atlas)[p + 2] = static_cast<uint8_t>(
        (1.f - t) * static_cast<float>((*atlas)[p + 2]) + t * ink_b);
    (*atlas)[p + 3] = 255;
  };
  for (int col = 0; col < atlas_dim; col += grid_step) {
    const bool major = (col % (grid_step * 4)) == 0;
    for (int row = 0; row < atlas_dim; ++row) {
      stamp_grid_px(col, row, major);
    }
  }
  for (int row = 0; row < atlas_dim; row += grid_step) {
    const bool major = (row % (grid_step * 4)) == 0;
    for (int col = 0; col < atlas_dim; ++col) {
      stamp_grid_px(col, row, major);
    }
  }

  // Rasterize inundation depth onto the atlas, then stroke isolines with the
  // shared vista DEM contour path (same ink as china elevation overlays).
  std::vector<float> depth_field(static_cast<size_t>(atlas_dim) *
                                     static_cast<size_t>(atlas_dim),
                                 0.f);
  int wet_samples = 0;
  const bool have_depth =
      depth && geotransform && dem_width > 1 && dem_height > 1;
  if (have_depth) {
    const double gt0 = geotransform[0];
    const double gt1 = geotransform[1];
    const double gt2 = geotransform[2];
    const double gt3 = geotransform[3];
    const double gt4 = geotransform[4];
    const double gt5 = geotransform[5];
    const double det = gt1 * gt5 - gt2 * gt4;
    if (std::abs(det) > 1.0e-18) {
      for (int row = 0; row < atlas_dim; ++row) {
        for (int col = 0; col < atlas_dim; ++col) {
          const double u =
              (static_cast<double>(col) + 0.5) / static_cast<double>(atlas_dim);
          const double v =
              (static_cast<double>(row) + 0.5) / static_cast<double>(atlas_dim);
          const double lon = min_lon + u * span_lon;
          const double lat = min_lat + v * span_lat;
          const double dx = lon - gt0;
          const double dy = lat - gt3;
          const double col_f = (gt5 * dx - gt2 * dy) / det;
          const double row_f = (-gt4 * dx + gt1 * dy) / det;
          const int dc = static_cast<int>(std::floor(col_f));
          const int dr = static_cast<int>(std::floor(row_f));
          if (dc < 0 || dr < 0 || dc >= dem_width || dr >= dem_height) {
            continue;
          }
          const float d =
              depth[static_cast<size_t>(dr) * static_cast<size_t>(dem_width) +
                    static_cast<size_t>(dc)];
          if (!(d > 0.05f) || !std::isfinite(d)) {
            continue;
          }
          // bake_elevation_overlay_rgba skips h < 1 m (ocean); lift depths.
          depth_field[static_cast<size_t>(row) * atlas_dim +
                      static_cast<size_t>(col)] = d + 1.5f;
          ++wet_samples;
        }
      }
    }
  }
  // Fallback radial field so isolines remain when depth is sparse/empty.
  if (wet_samples < atlas_dim) {
    const float cx = 0.5f * static_cast<float>(atlas_dim - 1);
    const float cy = 0.5f * static_cast<float>(atlas_dim - 1);
    const float inv = 1.f / (std::max)(1.f, cx);
    for (int row = 0; row < atlas_dim; ++row) {
      for (int col = 0; col < atlas_dim; ++col) {
        const float dx = static_cast<float>(col) - cx;
        const float dy = static_cast<float>(row) - cy;
        const float r = std::sqrt(dx * dx + dy * dy) * inv;
        depth_field[static_cast<size_t>(row) * atlas_dim +
                    static_cast<size_t>(col)] = 2.f + (1.f - r) * 40.f;
      }
    }
  }

  std::vector<uint8_t> contour_rgba;
  // Wider interval → fewer isolines (dense curves also read as thick bands).
  constexpr float kContourIntervalM = 6.f;
  if (!vista::bake_elevation_overlay_rgba(depth_field.data(), atlas_dim,
                                          atlas_dim, /*surface=*/false,
                                          /*curves=*/true, kContourIntervalM,
                                          &contour_rgba) ||
      contour_rgba.size() != atlas->size()) {
    return;
  }
  // Composite isolines as single-texel soft ink. Vista marks every 5th level
  // "thick" (+1x/+1y); drop those extras so majors stay one texel.
  const int dim = atlas_dim;
  auto contour_a = [&](int c, int r) -> uint8_t {
    if (c < 0 || r < 0 || c >= dim || r >= dim) {
      return 0;
    }
    return contour_rgba[(static_cast<size_t>(r) * dim + static_cast<size_t>(c)) *
                            4u +
                        3u];
  };
  for (int row = 0; row < dim; ++row) {
    for (int col = 0; col < dim; ++col) {
      if (contour_a(col, row) == 0) {
        continue;
      }
      if (contour_a(col - 1, row) != 0 && contour_a(col, row - 1) == 0 &&
          contour_a(col + 1, row) == 0) {
        continue;
      }
      if (contour_a(col, row - 1) != 0 && contour_a(col - 1, row) == 0 &&
          contour_a(col, row + 1) == 0) {
        continue;
      }
      const size_t p =
          (static_cast<size_t>(row) * dim + static_cast<size_t>(col)) * 4u;
      constexpr float kContourBlend = 0.35f;
      (*atlas)[p] = static_cast<uint8_t>(
          (1.f - kContourBlend) * static_cast<float>((*atlas)[p]) +
          kContourBlend * 245.f);
      (*atlas)[p + 1] = static_cast<uint8_t>(
          (1.f - kContourBlend) * static_cast<float>((*atlas)[p + 1]) +
          kContourBlend * 250.f);
      (*atlas)[p + 2] = static_cast<uint8_t>(
          (1.f - kContourBlend) * static_cast<float>((*atlas)[p + 2]) +
          kContourBlend * 255.f);
      (*atlas)[p + 3] = 255;
    }
  }
}

void commit_water_overlay(Scene3dSink* sink,
                          content::Scene3dPresenter* scene3d,
                          const double* xyz,
                          int point_count,
                          const int* triangles,
                          int triangle_count,
                          const float* depth,
                          int dem_width,
                          int dem_height,
                          const double* geotransform) {
  std::vector<float> tin_geo(static_cast<size_t>(point_count) * 3u);
  for (int i = 0; i < point_count; ++i) {
    tin_geo[static_cast<size_t>(i) * 3u] = static_cast<float>(xyz[i * 3]);
    tin_geo[static_cast<size_t>(i) * 3u + 1u] =
        static_cast<float>(xyz[i * 3 + 1]);
    tin_geo[static_cast<size_t>(i) * 3u + 2u] =
        static_cast<float>(xyz[i * 3 + 2]) + 1.5f;
  }
  const int index_count = triangle_count * 3;
  std::vector<unsigned> tin_idx(static_cast<size_t>(index_count));
  for (int i = 0; i < index_count; ++i) {
    if (triangles[i] < 0) {
      if (scene3d) {
        scene3d->clear_overlay_tin_mesh();
      } else if (sink) {
        sink->clear_overlay_tin_mesh();
      }
      return;
    }
    tin_idx[static_cast<size_t>(i)] = static_cast<unsigned>(triangles[i]);
  }

  // Keep water_like albedo (software paint / ocean-plane skip). Atlas carries
  // surface grid + depth isolines so the free-surface is not a solid wash.
  constexpr uint8_t kWaterAlbedo[4] = {28, 210, 245, 250};
  // 512² + soft-blend ink: opaque 128² stamps read as ~4.5px bands; opaque
  // 512² still ~3px FWHM; low-contrast single-texel blend targets ~1px.
  constexpr int kAtlas = 512;
  std::vector<uint8_t> atlas;
  std::vector<float> uv;
  bake_water_surface_atlas(xyz, point_count, depth, dem_width, dem_height,
                           geotransform, &atlas, kAtlas, &uv);

  if (scene3d) {
    scene3d->set_overlay_tin_mesh(tin_geo.data(), point_count, tin_idx.data(),
                                  index_count, kWaterAlbedo);
    scene3d->set_overlay_tin_drape(atlas.data(), static_cast<uint32_t>(kAtlas),
                                   static_cast<uint32_t>(kAtlas), uv.data(),
                                   static_cast<int>(uv.size()));
    // Atlas hairlines own the surface grid; triangle wireframe reads too heavy.
    scene3d->gpu().set_wireframe_enabled(false);
  } else if (sink) {
    sink->set_overlay_tin_mesh(tin_geo.data(), point_count, tin_idx.data(),
                               index_count, kWaterAlbedo);
    sink->set_overlay_tin_drape(atlas.data(), static_cast<uint32_t>(kAtlas),
                                static_cast<uint32_t>(kAtlas), uv.data(),
                                static_cast<int>(uv.size()));
  }
}

}  // namespace

bool present_stormsurge_water_mesh(content::GisDocument* doc,
                                   Scene3dSink* sink,
                                   content::Scene3dPresenter* scene3d,
                                   const double* xyz,
                                   int point_count,
                                   const int* triangles,
                                   int triangle_count,
                                   const float* depth,
                                   int dem_width,
                                   int dem_height,
                                   const double* geotransform) {
  if (!doc || !xyz || point_count < 3 || !triangles || triangle_count < 1) {
    return false;
  }
  std::vector<double> flipped(static_cast<size_t>(point_count) * 3);
  for (int i = 0; i < point_count; ++i) {
    flipped[static_cast<size_t>(i) * 3] = xyz[i * 3];
    flipped[static_cast<size_t>(i) * 3 + 1] = -xyz[i * 3 + 1];
    flipped[static_cast<size_t>(i) * 3 + 2] = xyz[i * 3 + 2];
  }
  doc->remove_layer("Flood water 3D");
  doc->remove_layer("Storm surge water");
  if (!doc->add_triangle_mesh("Storm surge water", flipped.data(), point_count,
                              triangles, triangle_count)) {
    return false;
  }

  if (scene3d || sink) {
    commit_water_overlay(sink, scene3d, xyz, point_count, triangles,
                         triangle_count, depth, dem_width, dem_height,
                         geotransform);
  }
  if (sink) {
    sink->invalidate();
  }
  return true;
}

}  // namespace plugin
