// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_layout_build.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_batches.h"
#include "content/browser/present/map2d/frame/map2d_carto.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "gis/style/document/style_document.h"
#include "vista/component/map/shade/bake.h"
#include "vista/component/map/ir.h"
#include "vista/pass/map/pass.h"
#include "vista/terrain/dem/dem_raster.h"

namespace content {
namespace detail {
namespace {

bool env_flag_one(const char* key) {
  const char* e = std::getenv(key);
  return e && e[0] == '1' && e[1] == '\0';
}

// china_city "area" is the Layers panel "Land" row. Jet hillshade is the land
// surface, so it must follow that checkbox — otherwise Lines-only still paints
// the full DEM sheet.
bool china_land_layer_visible(const MapScene* scene) {
  if (!scene) {
    return true;
  }
  bool saw_area = false;
  for (const MapScene::Layer& layer : scene->layers()) {
    if (layer.name != "area") {
      continue;
    }
    saw_area = true;
    if (layer.visible) {
      return true;
    }
  }
  return !saw_area;
}

}  // namespace

bool build_map2d_layout(const Map2dLayoutParams& in, Map2dLayoutOutput* out) {
  if (!out || !in.scene || !in.frame || in.cam.width_px == 0 ||
      in.cam.height_px == 0) {
    return false;
  }
  *out = Map2dLayoutOutput{};
  BASE_TRACE_EVENT("layout", "map2d.layout");

  bool use_carto = true;
  const gis::style::StyleDocument* style =
      resolve_present_style(in.scene->style_document(), &use_carto);

  vista::Layout layout;
  vista::LayoutInput layout_in;
  layout_in.view = {in.cam.width_px, in.cam.height_px, in.cam.min_x,
                    in.cam.min_y, in.cam.max_x, in.cam.max_y};
  layout_in.style = style;
  layout_in.zoom = zoom_from_scale(in.frame->scale());
  vista::WindowsGlyphRasterizer windows_rasterizer;
  layout_in.metrics = &windows_rasterizer;
  layout_in.tiles = {};
  layout_in.layout_gen = in.layout_gen;
  layout_in.live_layout_gen = in.live_layout_gen;
  layout_in.retained_slices = in.retained_slices;

  // Soft-gate: MAP2D_NO_HILLSHADE=1 skips bake.
  // Product cold start (defer_china_seed) still finds china_dem.tif on disk
  // via find_sample_dem_path even with a demo-only document — skip until the
  // scene has China extent. Force with MAP2D_FORCE_HILLSHADE=1.
  const bool force_hillshade =
      base::switch_is_one("map2d-force-hillshade") ||
      env_flag_one("MAP2D_FORCE_HILLSHADE");
  const bool land_visible = china_land_layer_visible(in.scene);
  const bool no_hillshade_switch =
      base::switch_is_one("map2d-no-hillshade") ||
      env_flag_one("MAP2D_NO_HILLSHADE");
  const bool no_china_extent =
      !force_hillshade && in.scene && !in.scene->has_china_extent();
  const bool land_hidden = !force_hillshade && !land_visible;
  const bool skip_hillshade =
      no_hillshade_switch || no_china_extent || land_hidden;
  if (skip_hillshade) {
    std::fprintf(stderr,
                 "map2d: hillshade skip - switch=%d china_extent=%d "
                 "land_visible=%d force=%d\n",
                 no_hillshade_switch ? 1 : 0, no_china_extent ? 0 : 1,
                 land_visible ? 1 : 0, force_hillshade ? 1 : 0);
  }

  if (!skip_hillshade && in.hillshade_ready &&
      in.hillshade_slot.texture_key != 0) {
    layout_in.hillshade_tiles.push_back(in.hillshade_slot);
    layout_in.have_dem_clip = true;
    layout_in.dem_clip = in.hillshade_slot;
  }
  if (!skip_hillshade && layout_in.hillshade_tiles.empty()) {
    if (const gis::style::StyleLayer* hs =
            find_hillshade_layer(style, layout_in.zoom)) {
      const std::string dem_path = vista::find_sample_dem_path();
      if (dem_path.empty()) {
        std::fprintf(stderr, "map2d: hillshade skip - china_dem not found\n");
      } else {
        const auto hs_t0 = std::chrono::steady_clock::now();
        vista::HillshadeBake baked;
        {
          BASE_TRACE_EVENT("HillshadeBake", "startup");
          baked = vista::bake_hillshade_slot(dem_path, layout_in.zoom, *hs,
                                            kMap2dHillshadeTextureKey);
        }
        if (baked.ok && baked.width > 0 && baked.height > 0 &&
            !baked.rgba.empty()) {
          // Attach the jet sheet on the same layout that baked it. Deferring
          // to layout_build_count>0 left cream land (#f5f3e9) on the first
          // present; StaticReuse then held that white China until settle.
          layout_in.have_dem_clip = true;
          layout_in.dem_clip = baked.slot;
          layout_in.hillshade_tiles.push_back(baked.slot);
          out->baked_w = baked.width;
          out->baked_h = baked.height;
          out->hillshade_slot = baked.slot;
          out->baked_rgba = std::move(baked.rgba);
          std::fprintf(stderr,
                       "map2d: hillshade baked %dx%d from %s tiles=%zu "
                       "opacity=%.2f key=0x%08x\n",
                       out->baked_w, out->baked_h, dem_path.c_str(),
                       layout_in.hillshade_tiles.size(),
                       layout_in.hillshade_tiles.back().opacity,
                       layout_in.hillshade_tiles.back().texture_key);
        } else {
          std::fprintf(stderr,
                       "map2d: hillshade skip - bake failed ok=%d %dx%d "
                       "rgba=%zu path=%s\n",
                       baked.ok ? 1 : 0, baked.width, baked.height,
                       baked.rgba.size(), dem_path.c_str());
        }
        out->hillshade_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - hs_t0)
                .count();
      }
    } else {
      std::fprintf(stderr,
                   "map2d: hillshade skip - no style layer @ zoom=%.2f\n",
                   layout_in.zoom);
    }
  }

  const double map_scale = in.frame->scale();
  vista::LayerBatchSet batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    batches = visible_layer_batches(in.scene->layers(), use_carto, map_scale);
  }
  {
    BASE_TRACE_EVENT("build", "map2d.layout");
    vista::MapIR built = layout.build(layout_in, batches.batches);
    if (in.live_layout_gen &&
        in.live_layout_gen->load(std::memory_order_acquire) != in.layout_gen) {
      return true;
    }
    out->frame = std::move(built);
  }
  out->ok = true;
  return true;
}

}  // namespace detail
}  // namespace content
