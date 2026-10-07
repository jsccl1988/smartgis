// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/probe.h"

#include "plugin/runtime/host/capability/shell.h"
#include <windows.h>
#include <shellapi.h>

#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/app/content_main.h"
#include "content/public/event_bus.h"
#include "content/public/gis_contents.h"
#include "content/public/types.h"
#include "content/public/tool_session.h"
#include "content/renderer/renderer_main.h"
#include "gis/edit/memory_session.h"
#include "gis/style/document/style_document.h"
#include "gis/tile/provider/tile_provider.h"
#include "gpu/gpu.h"
#include "net/http/http.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/shell/dpi.h"
#include "base/trace/event/process_trace.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/view/view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <cwctype>

namespace plugin {

int scenario_edit_m0(HarnessShell& browser) {
content::ToolSession* host = browser.edit_tool_session();
if (!host || !host->workspace() || !host->edits()) {
  return 11;
}
if (!browser.run_tool_command("edit.append.point")) {
  return 12;
}
browser.mark("edit-point");
tool::Interaction* cur = host->workspace()->stack().current();
if (!cur || std::strcmp(cur->id(), "draw.point") != 0) {
  return 13;
}
content::InputEvent down{};
down.kind = content::InputEvent::Kind::kLDown;
down.x_px = 12;
down.y_px = 18;
if (!host->dispatch_input(down)) {
  return 14;
}
if (!host->edits()->can_undo()) {
  return 15;
}
{
  // �?FeatureGeom: DraftPipeline must commit map-CRS geometry on draw.*.
  auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
  if (!mem || mem->committed_count() < 1) {
    return 15;
  }
  const gis::FeatureMutation got =
      mem->committed_at(mem->committed_count() - 1);
  if (got.op != gis::EditOp::kAppend || got.geom.empty() ||
      got.geom.kind != gis::FeatureGeom::Kind::kPoint ||
      got.geom.points.size() != 1) {
    return 15;
  }
  browser.mark("input-point-ok");
}
ui::views::StatusBar* status = browser.status_bar();
if (!status || status->status().find("Committed") == std::string::npos) {
  return 16;
}
if (!browser.run_tool_command("selection.point")) {
  return 17;
}
cur = host->workspace()->stack().current();
if (!cur || std::strcmp(cur->id(), "select.point") != 0) {
  return 18;
}
if (!browser.run_tool_command("selection.clear")) {
  return 19;
}
if (!status || status->status().find("Selection cleared") ==
                   std::string::npos) {
  return 20;
}
browser.mark("selection-ok");

// M0: append linestring 脙垄脗聠?FeatureInfo 脙垄脗聠?write_path roundtrip.
{
  const size_t before = browser.document()->feature_count();
  if (!browser.run_tool_command("edit.append.linestring")) {
    browser.detach_views();
    return 60;
  }
  tool::Interaction* line_tool = host->workspace()->stack().current();
  if (!line_tool ||
      std::strcmp(line_tool->id(), "draw.linestring") != 0) {
    browser.detach_views();
    return 60;
  }
  content::InputEvent v0{};
  v0.kind = content::InputEvent::Kind::kLDown;
  v0.x_px = 20;
  v0.y_px = 20;
  content::InputEvent v1{};
  v1.kind = content::InputEvent::Kind::kLDown;
  v1.x_px = 80;
  v1.y_px = 60;
  content::InputEvent fin{};
  fin.kind = content::InputEvent::Kind::kRDown;
  fin.x_px = 80;
  fin.y_px = 60;
  if (!host->dispatch_input(v0) || !host->dispatch_input(v1) ||
      !host->dispatch_input(fin)) {
    browser.detach_views();
    return 60;
  }
  if (browser.document()->feature_count() <= before) {
    browser.detach_views();
    return 60;
  }
  {
    auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
    if (!mem || mem->committed_count() < 1) {
      browser.detach_views();
      return 60;
    }
    const gis::FeatureMutation got =
        mem->committed_at(mem->committed_count() - 1);
    if (got.geom.empty() ||
        got.geom.kind != gis::FeatureGeom::Kind::kLineString ||
        got.geom.points.size() < 2) {
      browser.detach_views();
      return 60;
    }
    browser.mark("input-line-ok");
  }
  browser.mark("m0-line-ok");

  // Prefer the linestring just committed ?tokens.back() can be an earlier
  // seed/point row after fill_attribute_rows, which makes copy_feature_xy
  // return a single vertex and fail the round-trip (exit 62).
  content::FeatureId pick{};
  for (const content::GisScene::Layer& layer :
       browser.document()->layers()) {
    for (const content::GisScene::Feature& feature : layer.features) {
      if (feature.kind == content::GisScene::GeomKind::kLine &&
          feature.points.size() >= 2) {
        pick = feature.id;
      }
    }
  }
  if (pick.len == 0) {
    browser.detach_views();
    return 61;
  }
  if (!browser.document()->select_feature(pick)) {
    browser.detach_views();
    return 61;
  }
  browser.refresh_inspectors();
  ui::views::FeatureInfo* info = browser.feature_info();
  if (!info || info->feature_id().empty()) {
    browser.detach_views();
    return 61;
  }
  browser.mark("m0-featureinfo-ok");

  char tmp[MAX_PATH] = {};
  if (GetTempPathA(MAX_PATH, tmp) == 0) {
    browser.detach_views();
    return 62;
  }
  // Unique name: parallel te / leftover hosts must not share one path.
  char out[MAX_PATH] = {};
  if (sprintf_s(out, "%ssmartgis_m0_%lu_%lu.geojson", tmp,
                static_cast<unsigned long>(GetCurrentProcessId()),
                static_cast<unsigned long>(GetTickCount())) <= 0) {
    browser.detach_views();
    return 62;
  }
  DeleteFileA(out);
  // Bound the GeoJSON round-trip: seed_default may have loaded china_city
  // MultiPolygons (multi-MB). Re-exporting that layer via GDAL exceeds the
  // exe_smoke --self-test budget (~120s) and never reaches m0-save-ok.
  {
    std::vector<std::pair<double, double>> xy;
    if (!browser.document()->copy_feature_xy(pick, &xy) || xy.size() < 2) {
      browser.detach_views();
      return 62;
    }
    browser.document()->clear();
    if (!browser.document()->create_layer("m0_roundtrip", "LineString")) {
      browser.detach_views();
      return 62;
    }
    tool::Draft draft;
    draft.kind = tool::DraftKind::kLineString;
    draft.points.reserve(xy.size());
    for (size_t i = 0; i < xy.size(); ++i) {
      // Index carried in x_px; to_map restores double map coords.
      draft.points.push_back({static_cast<int32_t>(i), 0});
    }
    const content::FeatureId rebuilt = browser.document()->append_from_draft(
        draft, "draw.linestring",
        [&xy](int view_x, int, double* map_x, double* map_y) {
          const size_t i = static_cast<size_t>(view_x);
          if (i >= xy.size() || !map_x || !map_y) {
            return;
          }
          *map_x = xy[i].first;
          *map_y = xy[i].second;
        });
    if (rebuilt.len == 0) {
      browser.detach_views();
      return 62;
    }
  }
  if (!browser.document()->write_path(out)) {
    browser.detach_views();
    return 62;
  }
  content::GisScene probe;
  if (!probe.open_path(out) || probe.feature_count() < 1) {
    DeleteFileA(out);
    browser.detach_views();
    return 63;
  }
  DeleteFileA(out);
  browser.mark("m0-save-ok");
}

// Input interaction: draw.polygon ?FeatureGeom ring (�?append path).
{
  const size_t before = browser.document()->feature_count();
  if (!browser.run_tool_command("edit.append.polygon")) {
    browser.detach_views();
    return 64;
  }
  tool::Interaction* poly_tool = host->workspace()->stack().current();
  if (!poly_tool || std::strcmp(poly_tool->id(), "draw.polygon") != 0) {
    browser.detach_views();
    return 64;
  }
  content::InputEvent p0{};
  p0.kind = content::InputEvent::Kind::kLDown;
  p0.x_px = 30;
  p0.y_px = 30;
  content::InputEvent p1{};
  p1.kind = content::InputEvent::Kind::kLDown;
  p1.x_px = 90;
  p1.y_px = 30;
  content::InputEvent p2{};
  p2.kind = content::InputEvent::Kind::kLDown;
  p2.x_px = 60;
  p2.y_px = 80;
  content::InputEvent fin{};
  fin.kind = content::InputEvent::Kind::kRDown;
  fin.x_px = 60;
  fin.y_px = 80;
  if (!host->dispatch_input(p0) || !host->dispatch_input(p1) ||
      !host->dispatch_input(p2) || !host->dispatch_input(fin)) {
    browser.detach_views();
    return 64;
  }
  if (browser.document()->feature_count() <= before) {
    browser.detach_views();
    return 64;
  }
  auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
  if (!mem || mem->committed_count() < 1) {
    browser.detach_views();
    return 64;
  }
  const gis::FeatureMutation got =
      mem->committed_at(mem->committed_count() - 1);
  if (got.geom.empty() ||
      got.geom.kind != gis::FeatureGeom::Kind::kPolygon ||
      got.geom.points.size() < 3) {
    browser.detach_views();
    return 64;
  }
  browser.mark("input-poly-ok");
  browser.mark("input-ok");
}

if (!browser.run_tool_command("view.backend.maplibre")) {
  return 43;
}
if (!status || status->status().find("MapLibre") == std::string::npos) {
  return 44;
}
if (!browser.run_tool_command("view.backend.rhi")) {
  return 45;
}
if (!status || status->status().find("RHI") == std::string::npos) {
  return 46;
}
browser.mark("backend-ok");
  return 0;
}

}  // namespace plugin
