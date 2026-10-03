// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/seed/orthogrid_mesh.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/document/map_scene.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/product/orthogrid/detail/boundary_solve.h"
#include "tool/draft/draft.h"

#include <cstdio>
#include <utility>
#include <vector>
#include <windows.h>

namespace app {
namespace detail {

bool load_map2d_orthogrid_mesh(Browser& browser) {
  if (!browser.document()) {
    return false;
  }
  char bnd_path[MAX_PATH] = {};
  {
    wchar_t base[MAX_PATH] = {};
    if (!app::detail::exe_dir_with_slash(base, MAX_PATH)) {
      return false;
    }
    const wchar_t* rels[] = {L"..\\data\\plugin\\orthogrid_sample.gridbnd",
                             L"data\\plugin\\orthogrid_sample.gridbnd"};
    bool found = false;
    for (const wchar_t* rel : rels) {
      wchar_t full[MAX_PATH] = {};
      if (wcscpy_s(full, base) != 0 || wcscat_s(full, rel) != 0) {
        continue;
      }
      if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
        continue;
      }
      if (WideCharToMultiByte(CP_UTF8, 0, full, -1, bnd_path,
                              static_cast<int>(sizeof(bnd_path)), nullptr,
                              nullptr) <= 0) {
        continue;
      }
      found = true;
      break;
    }
    if (!found) {
      std::fprintf(stderr, "map2d-showcase: missing orthogrid_sample.gridbnd\n");
      return false;
    }
  }
  // Thompson steps help orthogonality on the irregular coastal Dirichlet.
  constexpr int kEllipticIters = 4;
  const plugin::detail::BoundarySolve solved =
      plugin::detail::solve_grid_boundary_file(bnd_path, kEllipticIters);
  if (!solved.ok || solved.nx < 3 || solved.ny < 3 ||
      solved.xs.size() != static_cast<size_t>(solved.nx * solved.ny)) {
    return false;
  }

  plugin::OrthogridMeshCommit commit;
  commit.nx = solved.nx;
  commit.ny = solved.ny;
  commit.xs = solved.xs.data();
  commit.ys = solved.ys.data();
  if (!solved.cell_orth.empty()) {
    commit.cell_orth = solved.cell_orth.data();
  }
  if (!solved.raster_orth.empty() && solved.raster_w > 0 &&
      solved.raster_h > 0) {
    commit.raster_w = solved.raster_w;
    commit.raster_h = solved.raster_h;
    commit.raster_min_x = solved.raster_min_x;
    commit.raster_min_y = solved.raster_min_y;
    commit.raster_max_x = solved.raster_max_x;
    commit.raster_max_y = solved.raster_max_y;
    commit.raster_orth = solved.raster_orth.data();
  }
  if (plugin::publish_orthogrid_mesh(commit)) {
    return browser.document()->feature_count() >= 2;
  }

  // Fallback when mesh writer is unset (unit harness without Browser::init).
  browser.document()->clear();
  browser.document()->clear_style_document();
  if (!browser.document()->create_layer("orthogrid_extent", "Polygon")) {
    return false;
  }
  {
    tool::Draft extent;
    extent.kind = tool::DraftKind::kPolygon;
    extent.points = {{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}};
    browser.document()->append_from_draft(
        extent, "draw.polygon",
        [](int vx, int vy, double* map_x, double* map_y) {
          *map_x = static_cast<double>(vx) / 1000.0;
          *map_y = -static_cast<double>(vy) / 1000.0;
        });
  }
  if (!browser.document()->create_layer("orthogrid", "LineString")) {
    return false;
  }

  auto append_polyline = [&](const std::vector<std::pair<double, double>>& xy) {
    if (xy.size() < 2) {
      return false;
    }
    tool::Draft draft;
    draft.kind = tool::DraftKind::kLineString;
    draft.points.reserve(xy.size());
    for (size_t i = 0; i < xy.size(); ++i) {
      draft.points.push_back({static_cast<int32_t>(i), 0});
    }
    const content::FeatureId id = browser.document()->append_from_draft(
        draft, "draw.linestring",
        [&xy](int view_x, int, double* map_x, double* map_y) {
          const size_t i = static_cast<size_t>(view_x);
          if (i >= xy.size() || !map_x || !map_y) {
            return;
          }
          *map_x = xy[i].first;
          *map_y = -xy[i].second;
        });
    if (id.len != 0) {
      browser.document()->update_feature_field(
          content::MapScene::feature_token(id), "type", "highway");
    }
    return id.len != 0;
  };

  const int nx = solved.nx;
  const int ny = solved.ny;
  for (int j = 0; j < ny; ++j) {
    std::vector<std::pair<double, double>> row;
    row.reserve(static_cast<size_t>(nx));
    for (int i = 0; i < nx; ++i) {
      const size_t at = static_cast<size_t>(j * nx + i);
      row.emplace_back(solved.xs[at], solved.ys[at]);
    }
    if (!append_polyline(row)) {
      return false;
    }
  }
  for (int i = 0; i < nx; ++i) {
    std::vector<std::pair<double, double>> col;
    col.reserve(static_cast<size_t>(ny));
    for (int j = 0; j < ny; ++j) {
      const size_t at = static_cast<size_t>(j * nx + i);
      col.emplace_back(solved.xs[at], solved.ys[at]);
    }
    if (!append_polyline(col)) {
      return false;
    }
  }
  return browser.document()->feature_count() >= 2;
}


}  // namespace detail
}  // namespace app
