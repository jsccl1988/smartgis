// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scheduler: GisScene / ViewFrame and process switches in, vista layout out.
// Tile math, carto resolve, batch build, hillshade attach, and Layout::build
// live under vista/component/map.

#include "content/browser/present/map2d/frame/layout/map2d_layout_build.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/frame/batches/map2d_batches.h"
#include "content/browser/present/map2d/frame/carto/map2d_carto.h"
#include "content/browser/present/map2d/frame/tile/map2d_tile_math.h"
#include "vista/component/map/ir.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <utility>

#include "base/core/log.h"
#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "vista/component/map/shade/attach.h"
#include "vista/pass/map/pass.h"

namespace content {
namespace detail {
namespace {

bool env_flag_one(const char* key) {
  const char* e = std::getenv(key);
  return e && e[0] == '1' && e[1] == '\0';
}

void log_hillshade_attach(const vista::HillshadeAttachResult& shade) {
  switch (shade.kind) {
    case vista::HillshadeAttachKind::kDemMissing:
      LOGGING(LOG_WARNING, "map2d: hillshade skip - dem not found");
      break;
    case vista::HillshadeAttachKind::kBaked:
      LOGGING(LOG_INFO,
              "map2d: hillshade baked %dx%d from %s tiles=%zu "
              "opacity=%.2f key=0x%08x",
              shade.width, shade.height, shade.dem_path.c_str(),
              shade.tile_count, shade.opacity, shade.texture_key);
      break;
    case vista::HillshadeAttachKind::kBakeFailed:
      LOGGING(LOG_WARNING,
              "map2d: hillshade skip - bake failed ok=%d %dx%d "
              "rgba=%zu path=%s",
              shade.bake_ok ? 1 : 0, shade.width, shade.height,
              shade.rgba_bytes, shade.dem_path.c_str());
      break;
    case vista::HillshadeAttachKind::kNoLayer:
      LOGGING(LOG_INFO,
              "map2d: hillshade skip - no style layer @ zoom=%.2f",
              shade.zoom);
      break;
    case vista::HillshadeAttachKind::kSkipped:
      LOGGING(LOG_INFO, "map2d: hillshade skipped (policy.skip) tiles=%zu",
              shade.tile_count);
      break;
    case vista::HillshadeAttachKind::kReused:
      LOGGING(LOG_INFO,
              "map2d: hillshade reused tiles=%zu opacity=%.2f key=0x%08x",
              shade.tile_count, shade.opacity, shade.texture_key);
      break;
  }
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

  vista::LayoutInput layout_in;
  // CameraKey comes from ViewFrame::view_world_extent, which already returns
  // geographic +lat (unflipped). layers_to_pod and DEM slots are also +lat.
  // Do not negate cam Y here — a second flip put view in -lat while geometry
  // stayed +lat, so emit_hillshade / fills culled to frame_items=0.
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
  // Catalog style.hillshade visibility always wins (isolate vectors).
  // Scene capability (extent + visible land role + resolvable DEM) gates
  // attach so empty/demo docs do not bake a sample DEM. Force with
  // MAP2D_FORCE_HILLSHADE=1 bypasses the land-visible check only.
  // First layout (layout_build_count==0) may defer DEM bake so cold
  // layout_ms is tess-only; the cache forces one follow-up.
  const bool force_hillshade =
      base::switch_is_one("map2d-force-hillshade") ||
      env_flag_one("MAP2D_FORCE_HILLSHADE");
  const bool no_hillshade_switch =
      base::switch_is_one("map2d-no-hillshade") ||
      env_flag_one("MAP2D_NO_HILLSHADE");
  const bool ui_hillshade_off =
      in.scene != nullptr && !in.scene->hillshade_visible();
  const bool eligible =
      in.scene != nullptr &&
      (force_hillshade
           ? (in.scene->has_drawable_extent() && in.scene->has_resolvable_dem())
           : in.scene->can_attach_hillshade());
  const bool ineligible = !eligible;
  const bool defer_first_bake = in.allow_defer_hillshade && !force_hillshade &&
                                !in.hillshade_ready &&
                                in.layout_build_count == 0 &&
                                !no_hillshade_switch && !ui_hillshade_off &&
                                !ineligible;
  const bool skip_hillshade = no_hillshade_switch || ui_hillshade_off ||
                              ineligible || defer_first_bake;

  if (defer_first_bake) {
    out->deferred_hillshade = true;
    LOGGING(LOG_INFO,
            "map2d: hillshade defer - first layout tess-only "
            "(follow-up attach)");
  } else if (skip_hillshade) {
    LOGGING(LOG_INFO,
            "map2d: hillshade skip - switch=%d ui_off=%d ineligible=%d "
            "force=%d",
            no_hillshade_switch ? 1 : 0, ui_hillshade_off ? 1 : 0,
            ineligible ? 1 : 0, force_hillshade ? 1 : 0);
  }

  vista::HillshadeAttachPolicy shade_policy;
  shade_policy.skip = skip_hillshade;
  // Do not reuse a prior bake slot when the Catalog leaf hid the sheet —
  // otherwise land fill stays suppressed (jet_covers_view) with no raster.
  shade_policy.ready = in.hillshade_ready && !skip_hillshade;
  shade_policy.ready_slot = in.hillshade_slot;
  shade_policy.texture_key = kMap2dHillshadeTextureKey;
  vista::HillshadeAttachResult shade =
      vista::attach_hillshade_slot(&layout_in, shade_policy);
  log_hillshade_attach(shade);
  if (shade.kind == vista::HillshadeAttachKind::kBaked) {
    out->baked_w = shade.width;
    out->baked_h = shade.height;
    out->hillshade_slot = shade.slot;
    out->baked_rgba = std::move(shade.rgba);
  }
  if (shade.kind == vista::HillshadeAttachKind::kBaked ||
      shade.kind == vista::HillshadeAttachKind::kBakeFailed) {
    out->hillshade_ms = shade.elapsed_ms;
  }

  vista::LayerBatchSet batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    std::lock_guard<std::recursive_mutex> layer_lock(in.scene->layer_mutex());
    batches = visible_layer_batches(in.scene->layers(), use_carto,
                                    in.frame->scale());
  }
  {
    size_t geom_n = 0;
    for (const vista::LayerBatch& b : batches.batches) {
      geom_n += b.geoms.size();
    }
    std::fprintf(stderr,
                 "map2d: layout batches=%zu geoms=%zu use_carto=%d "
                 "cam=[%.3f,%.3f]-[%.3f,%.3f] view=[%.3f,%.3f]-[%.3f,%.3f] "
                 "hs_tiles=%zu\n",
                 batches.batches.size(), geom_n, use_carto ? 1 : 0, in.cam.min_x,
                 in.cam.min_y, in.cam.max_x, in.cam.max_y, layout_in.view.min_x,
                 layout_in.view.min_y, layout_in.view.max_x,
                 layout_in.view.max_y, layout_in.hillshade_tiles.size());
    if (!layout_in.hillshade_tiles.empty()) {
      const vista::TileSlot& hs = layout_in.hillshade_tiles.front();
      std::fprintf(stderr, "map2d: hs_slot=[%.3f,%.3f]-[%.3f,%.3f]\n", hs.min_x,
                   hs.min_y, hs.max_x, hs.max_y);
    }
    std::fflush(stderr);
  }

  vista::HeatmapBake heatmap_bake;
  vista::layout_bind_heatmap_bake(&heatmap_bake);

  vista::Layout layout;
  {
    BASE_TRACE_EVENT("build", "map2d.layout");
    vista::MapIR built = layout.build(layout_in, batches.batches);
    vista::layout_bind_heatmap_bake(nullptr);
    if (in.live_layout_gen &&
        in.live_layout_gen->load(std::memory_order_acquire) != in.layout_gen) {
      // Superseded by bind/invalidate/a newer rebuild — do not publish.
      out->ok = false;
      return false;
    }
    if (!heatmap_bake.rgba.empty() && heatmap_bake.width > 0 &&
        heatmap_bake.height > 0) {
      out->heatmap_w = heatmap_bake.width;
      out->heatmap_h = heatmap_bake.height;
      out->heatmap_rgba = std::move(heatmap_bake.rgba);
    }
    out->frame = std::move(built);
  }
  out->ok = true;
  return true;
}

}  // namespace detail
}  // namespace content
