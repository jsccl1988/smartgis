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

#include <chrono>
#include <cstdio>
#include <cstdlib>
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

// #region agent log
void agent_dbg_ndjson(const char* hypothesis_id, const char* location,
                      const char* message, const char* data_json) {
  if (FILE* f =
          std::fopen("C:/Dev/src/gis/smartgis/debug-547690.log", "a")) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    std::fprintf(f,
                 "{\"sessionId\":\"547690\",\"runId\":\"pre-fix\","
                 "\"hypothesisId\":\"%s\",\"location\":\"%s\","
                 "\"message\":\"%s\",\"data\":%s,\"timestamp\":%lld}\n",
                 hypothesis_id, location, message, data_json,
                 static_cast<long long>(ms));
    std::fclose(f);
  }
}
// #endregion

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
  // #region agent log
  {
    char data[512];
    std::snprintf(
        data, sizeof(data),
        "{\"kind\":%d,\"tiles\":%zu,\"w\":%d,\"h\":%d,\"key\":%u,"
        "\"bake_ok\":%d,\"rgba\":%zu}",
        static_cast<int>(shade.kind), shade.tile_count, shade.width,
        shade.height, shade.texture_key, shade.bake_ok ? 1 : 0,
        shade.rgba_bytes);
    agent_dbg_ndjson("C", "map2d_layout_build.cc:log_hillshade_attach",
                     "attach_result", data);
  }
  // #endregion
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
  // Scene capability (extent + visible land role + resolvable DEM) gates
  // attach so empty/demo docs do not bake a sample DEM. Force with
  // MAP2D_FORCE_HILLSHADE=1. First layout (layout_build_count==0) may defer
  // DEM bake so cold layout_ms is tess-only; the cache forces one follow-up.
  const bool force_hillshade =
      base::switch_is_one("map2d-force-hillshade") ||
      env_flag_one("MAP2D_FORCE_HILLSHADE");
  const bool no_hillshade_switch =
      base::switch_is_one("map2d-no-hillshade") ||
      env_flag_one("MAP2D_NO_HILLSHADE");
  const bool ineligible =
      !force_hillshade && in.scene && !in.scene->can_attach_hillshade();
  const bool defer_first_bake = in.allow_defer_hillshade && !force_hillshade &&
                                !in.hillshade_ready &&
                                in.layout_build_count == 0 &&
                                !no_hillshade_switch && !ineligible;
  const bool skip_hillshade =
      no_hillshade_switch || ineligible || defer_first_bake;

  // #region agent log
  {
    size_t layer_n = 0;
    size_t vis_n = 0;
    size_t land_n = 0;
    size_t land_vis_n = 0;
    char names[256] = {};
    size_t names_used = 0;
    if (in.scene) {
      for (const GisScene::Layer& layer : in.scene->layers()) {
        ++layer_n;
        if (layer.visible) {
          ++vis_n;
        }
        if (layer.role == LayerRole::kLandSurface) {
          ++land_n;
          if (layer.visible) {
            ++land_vis_n;
          }
        }
        if (names_used + layer.name.size() + 4 < sizeof(names)) {
          if (names_used > 0) {
            names[names_used++] = ',';
          }
          for (char c : layer.name) {
            names[names_used++] = c;
          }
          names[names_used++] = layer.visible ? '+' : '-';
          names[names_used] = '\0';
        }
      }
    }
    const bool can_attach =
        in.scene != nullptr && in.scene->can_attach_hillshade();
    char data[768];
    std::snprintf(
        data, sizeof(data),
        "{\"force\":%d,\"can_attach\":%d,\"no_hs\":%d,\"ineligible\":%d,"
        "\"defer\":%d,\"skip\":%d,\"ready\":%d,\"build_n\":%llu,"
        "\"layers\":%zu,\"vis\":%zu,\"land_n\":%zu,\"land_vis_n\":%zu,"
        "\"names\":\"%s\",\"tiles_in\":%zu}",
        force_hillshade ? 1 : 0, can_attach ? 1 : 0,
        no_hillshade_switch ? 1 : 0, ineligible ? 1 : 0,
        defer_first_bake ? 1 : 0, skip_hillshade ? 1 : 0,
        in.hillshade_ready ? 1 : 0,
        static_cast<unsigned long long>(in.layout_build_count), layer_n, vis_n,
        land_n, land_vis_n, names, layout_in.hillshade_tiles.size());
    agent_dbg_ndjson("A", "map2d_layout_build.cc:build_map2d_layout",
                     "hillshade_gate", data);
  }
  // #endregion

  if (defer_first_bake) {
    out->deferred_hillshade = true;
    LOGGING(LOG_INFO,
            "map2d: hillshade defer - first layout tess-only "
            "(follow-up attach)");
  } else if (skip_hillshade) {
    LOGGING(LOG_INFO,
            "map2d: hillshade skip - switch=%d ineligible=%d force=%d",
            no_hillshade_switch ? 1 : 0, ineligible ? 1 : 0,
            force_hillshade ? 1 : 0);
  }

  vista::HillshadeAttachPolicy shade_policy;
  shade_policy.skip = skip_hillshade;
  shade_policy.ready = in.hillshade_ready;
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

  // #region agent log
  {
    char data[256];
    std::snprintf(data, sizeof(data),
                  "{\"tiles_out\":%zu,\"kind\":%d,\"deferred\":%d}",
                  layout_in.hillshade_tiles.size(),
                  static_cast<int>(shade.kind),
                  out->deferred_hillshade ? 1 : 0);
    agent_dbg_ndjson("D", "map2d_layout_build.cc:build_map2d_layout",
                     "tiles_after_attach", data);
  }
  // #endregion

  vista::LayerBatchSet batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    batches = visible_layer_batches(in.scene->layers(), use_carto,
                                    in.frame->scale());
  }

  vista::HeatmapBake heatmap_bake;
  layout_in.heatmap_bake = &heatmap_bake;

  vista::Layout layout;
  {
    BASE_TRACE_EVENT("build", "map2d.layout");
    vista::MapIR built = layout.build(layout_in, batches.batches);
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
    // #region agent log
    {
      size_t n_raster = 0;
      size_t n_line = 0;
      size_t n_fill = 0;
      for (const vista::DrawItem& it : built.items) {
        if (it.kind == vista::DrawKind::kRaster) {
          ++n_raster;
        } else if (it.kind == vista::DrawKind::kLine) {
          ++n_line;
        } else if (it.kind == vista::DrawKind::kFill) {
          ++n_fill;
        }
      }
      char data[192];
      std::snprintf(data, sizeof(data),
                    "{\"items\":%zu,\"raster\":%zu,\"line\":%zu,\"fill\":%zu}",
                    built.items.size(), n_raster, n_line, n_fill);
      agent_dbg_ndjson("E", "map2d_layout_build.cc:build_map2d_layout",
                       "mapir_counts", data);
    }
    // #endregion
    out->frame = std::move(built);
  }
  out->ok = true;
  return true;
}

}  // namespace detail
}  // namespace content
