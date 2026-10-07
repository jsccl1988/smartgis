// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scheduler: MapScene / ViewFrame and process switches in, vista layout out.
// Tile math, carto resolve, batch build, hillshade attach, and Layout::build
// live under vista/component/map.

#include "content/browser/present/map2d/frame/map2d_layout_build.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_batches.h"
#include "content/browser/present/map2d/frame/map2d_carto.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

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

void log_hillshade_attach(const vista::HillshadeAttachResult& shade) {
  switch (shade.kind) {
    case vista::HillshadeAttachKind::kDemMissing:
      std::fprintf(stderr, "map2d: hillshade skip - china_dem not found\n");
      break;
    case vista::HillshadeAttachKind::kBaked:
      std::fprintf(stderr,
                   "map2d: hillshade baked %dx%d from %s tiles=%zu "
                   "opacity=%.2f key=0x%08x\n",
                   shade.width, shade.height, shade.dem_path.c_str(),
                   shade.tile_count, shade.opacity, shade.texture_key);
      break;
    case vista::HillshadeAttachKind::kBakeFailed:
      std::fprintf(stderr,
                   "map2d: hillshade skip - bake failed ok=%d %dx%d "
                   "rgba=%zu path=%s\n",
                   shade.bake_ok ? 1 : 0, shade.width, shade.height,
                   shade.rgba_bytes, shade.dem_path.c_str());
      break;
    case vista::HillshadeAttachKind::kNoLayer:
      std::fprintf(stderr,
                   "map2d: hillshade skip - no style layer @ zoom=%.2f\n",
                   shade.zoom);
      break;
    case vista::HillshadeAttachKind::kSkipped:
    case vista::HillshadeAttachKind::kReused:
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

  vista::LayerBatchSet batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    batches = visible_layer_batches(in.scene->layers(), use_carto,
                                    in.frame->scale());
  }

  vista::Layout layout;
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
