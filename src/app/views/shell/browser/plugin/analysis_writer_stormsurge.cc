// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_stormsurge.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "app/views/shell/browser/plugin/analysis_writer_flood.h"
#include "app/views/shell/runtime/analysis/playback.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "gis/carto/style/style_document.h"
#include "plugin/product/stormsurge/commands.h"

#include <cstdint>
#include <vector>

namespace app {
namespace detail {
namespace {

constexpr const char* kStormSurgeStyleJson = R"json({
  "version": 8,
  "name": "stormsurge_mask",
  "layers": [
    {"id":"surge_cells","type":"fill","source-layer":"flood_mask",
     "paint":{"fill-color":"#16a085","fill-opacity":0.85}},
    {"id":"surge_edge","type":"line","source-layer":"flood_mask",
     "paint":{"line-color":"#0e6655","line-width":2.5}},
    {"id":"terrain","type":"line","source-layer":"flood_terrain",
     "paint":{"line-color":"#566573","line-width":2.0}},
    {"id":"water_fill","type":"fill","source-layer":"Storm surge water",
     "paint":{"fill-color":"#1abc9c","fill-opacity":0.55}},
    {"id":"water_edge","type":"line","source-layer":"Storm surge water",
     "paint":{"line-color":"#0e6655","line-width":1.5}}
  ]
})json";

}  // namespace

// Storm-surge mask: same flood-style paint; session product = kStormSurge.
// Map2d polygons only; scene3d free surface comes from water-mesh writer.
bool commit_stormsurge_mask(content::MapScene* doc,
                            BrowserUiDelegate* ui,
                            AnalysisPlayback* session,
                            content::Scene3dPresenter* scene3d,
                            const unsigned char* mask,
                            int width,
                            int height,
                            const double* geotransform,
                            int frame_index,
                            int frame_count,
                            double water_level) {
  if (!doc || !mask || !geotransform || width <= 0 || height <= 0 || !session) {
    return false;
  }
  if (frame_index == 0 ||
      session->product() != AnalysisProduct::kStormSurge) {
    session->begin_stormsurge(width, height, geotransform, water_level,
                              frame_count);
    doc->clear();
    (void)apply_style_json(doc, kStormSurgeStyleJson);
    // Drop stale water TIN until the mesh writer commits the new free surface.
    if (scene3d) {
      scene3d->clear_overlay_tin_mesh();
    }
  }
  session->push_flood_mask(mask, width, height, water_level);
  // Mask polygons only; water surface comes from set_stormsurge_water_mesh_writer.
  return paint_flood_mask_layer(doc, ui, mask, width, height, geotransform,
                                water_level, frame_index == 0,
                                /*add_water_standin=*/false);
}

// Wet-cell free-surface: map2d triangle layer (Y negated) + scene3d TIN overlay
// (geographic lon/lat/elev — peer of commit_mine_stratum overlay_tin_mesh).
// When |session| is set, the mesh is stored at |frame_index| for ResultPlayback.
bool commit_stormsurge_water_mesh(content::MapScene* doc,
                                  BrowserUiDelegate* ui,
                                  AnalysisPlayback* session,
                                  content::Scene3dPresenter* scene3d,
                                  const double* xyz,
                                  int point_count,
                                  const int* triangles,
                                  int triangle_count,
                                  int frame_index) {
  if (!doc || !xyz || point_count < 3 || !triangles || triangle_count < 1) {
    return false;
  }
  if (session && session->product() == AnalysisProduct::kStormSurge) {
    session->push_stormsurge_water_mesh(xyz, point_count, triangles,
                                        triangle_count, frame_index);
  }
  std::vector<double> flipped(static_cast<size_t>(point_count) * 3);
  for (int i = 0; i < point_count; ++i) {
    flipped[static_cast<size_t>(i) * 3] = xyz[i * 3];
    flipped[static_cast<size_t>(i) * 3 + 1] = -xyz[i * 3 + 1];
    flipped[static_cast<size_t>(i) * 3 + 2] = xyz[i * 3 + 2];
  }
  doc->remove_layer("Flood water 3D");
  doc->remove_layer("Storm surge water");
  if (!doc->add_triangle_layer("Storm surge water", flipped.data(), point_count,
                               triangles, triangle_count)) {
    return false;
  }

  if (scene3d) {
    std::vector<float> tin_geo(static_cast<size_t>(point_count) * 3u);
    for (int i = 0; i < point_count; ++i) {
      tin_geo[static_cast<size_t>(i) * 3u] =
          static_cast<float>(xyz[i * 3]);
      tin_geo[static_cast<size_t>(i) * 3u + 1u] =
          static_cast<float>(xyz[i * 3 + 1]);
      // Free-surface lift so the overlay clears DEM hypsometric paint.
      tin_geo[static_cast<size_t>(i) * 3u + 2u] =
          static_cast<float>(xyz[i * 3 + 2]) + 18.f;
    }
    const int index_count = triangle_count * 3;
    std::vector<unsigned> tin_idx(static_cast<size_t>(index_count));
    for (int i = 0; i < index_count; ++i) {
      if (triangles[i] < 0) {
        scene3d->clear_overlay_tin_mesh();
        return false;
      }
      tin_idx[static_cast<size_t>(i)] =
          static_cast<unsigned>(triangles[i]);
    }
    // Bright cyan free-surface (score water_on_land: b>=140 g>=120 sum>280).
    constexpr uint8_t kWaterAlbedo[4] = {28, 210, 245, 250};
    scene3d->set_overlay_tin_mesh(tin_geo.data(), point_count, tin_idx.data(),
                                  index_count, kWaterAlbedo);
  }
  return refresh_ui_after_layer(ui);
}

void wire_stormsurge_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_stormsurge_mask_writer(
      [browser](const unsigned char* mask, int width, int height,
             const double* geotransform, int frame_index, int frame_count,
             double water_level) {
        return commit_stormsurge_mask(&browser->session().document(), browser->ui(),
                                      &browser->analysis_playback(), &browser->session().scene3d(),
                                      mask, width, height, geotransform,
                                      frame_index, frame_count, water_level);
      });
  plugin::set_stormsurge_water_mesh_writer(
      [browser](const double* xyz, int point_count, const int* triangles,
             int triangle_count, int frame_index, int /*frame_count*/) {
        return commit_stormsurge_water_mesh(
            &browser->session().document(), browser->ui(), &browser->analysis_playback(),
            &browser->session().scene3d(), xyz, point_count, triangles, triangle_count,
            frame_index);
      });

}

}  // namespace detail
}  // namespace app
