// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/probe.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/map_contents.h"
#include "content/public/map_layer_types.h"
#include "content/public/view_host.h"
#include "gis/edit/memory_session.h"
#include "render/rhi/rhi.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

const char* geom_kind_token(gis::FeatureGeom::Kind kind) {
  switch (kind) {
    case gis::FeatureGeom::Kind::kPoint:
      return "point";
    case gis::FeatureGeom::Kind::kLineString:
      return "linestring";
    case gis::FeatureGeom::Kind::kPolygon:
      return "polygon";
    default:
      return "";
  }
}

content::ViewHost* tool_stack_host(Browser& browser) {
  content::ViewHost* host = nullptr;
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    if (scene->is_visible()) {
      host = browser.scene_host();
    }
  }
  if (!host) {
    if (ui::views::DrawHost* data = browser.data_draw_host()) {
      if (data->is_visible()) {
        host = browser.data_host();
      }
    }
  }
  if (!host) {
    host = browser.edit_view_host();
  }
  return host;
}

}  // namespace

content::ViewHost* active_map_host(Browser& browser) {
  if (browser.ui()) {
    return browser.ui()->active_view_host();
  }
  return browser.edit_view_host();
}

int activate_view_tool(Browser& browser, const std::string& id) {
  content::ViewHost* host = active_map_host(browser);
  if (!host || !host->workspace()) {
    return 21;
  }
  if (id.empty() || !host->activate(id)) {
    return 22;
  }
  return 0;
}

bool fill_edit_host_status(Browser& browser, content::EditHostStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  content::ViewHost* host = browser.edit_view_host();
  if (!host) {
    return true;
  }
  out->host_present = 1;
  if (host->workspace()) {
    out->workspace = 1;
  }
  if (host->edits()) {
    out->edits = 1;
    if (dynamic_cast<gis::MemoryEditSession*>(host->edits()) != nullptr) {
      out->memory_session = 1;
    }
  }
  return true;
}

bool fill_map_ready_status(Browser& browser,
                           int timeout_ms,
                           content::MapReadyStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  ui::views::DrawHost* map = browser.draw_host();
  if (!map) {
    return true;
  }
  out->map_present = 1;
  const auto snapshot = [&]() {
    content::Map2dPresenter* map2d = browser.map2d();
    if (!map2d) {
      out->layout_built = 0;
      out->layout_build_count = 0;
      return;
    }
    out->layout_build_count = static_cast<int>(map2d->layout_build_count());
    out->layout_built = out->layout_build_count > 0 ? 1 : 0;
  };
  if (timeout_ms <= 0) {
    snapshot();
    return true;
  }
  const DWORD budget = static_cast<DWORD>(timeout_ms);
  const DWORD t0 = GetTickCount();
  for (;;) {
    snapshot();
    if (out->layout_built) {
      return true;
    }
    if (GetTickCount() - t0 >= budget) {
      (void)map->wait_ready(1);
      snapshot();
      return true;
    }
    pump_messages(50);
  }
}

bool fill_tool_status(Browser& browser, content::ToolStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  content::ViewHost* host = tool_stack_host(browser);
  if (!host || !host->workspace()) {
    return true;
  }
  tool::Interaction* cur = host->workspace()->stack().current();
  if (!cur || !cur->id()) {
    return true;
  }
  out->id = cur->id();
  return true;
}

bool fill_last_geom(Browser& browser, content::GeomStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  content::ViewHost* host = browser.edit_view_host();
  if (!host || !host->edits()) {
    return true;
  }
  auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
  if (!mem) {
    return true;
  }
  out->committed = static_cast<int>(mem->committed_count());
  if (out->committed < 1) {
    return true;
  }
  const gis::FeatureMutation got =
      mem->committed_at(static_cast<size_t>(out->committed - 1));
  out->is_append = got.op == gis::EditOp::kAppend ? 1 : 0;
  if (got.geom.empty()) {
    return true;
  }
  out->kind = geom_kind_token(got.geom.kind);
  out->point_count = static_cast<int>(got.geom.points.size());
  return true;
}

bool fill_view_scale(Browser& browser, content::ViewScaleStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  if (!browser.edit_view_host() || !browser.view_frame() ||
      !browser.orbit_frame()) {
    return true;
  }
  out->has_frame = 1;
  out->scale = browser.view_frame()->scale();
  const render::rhi::CameraMatrices ortho =
      browser.orbit_frame()->camera_matrices_ortho(800.f, 600.f);
  out->ortho = ortho.kind == render::rhi::CameraKind::kOrtho ? 1 : 0;
  return true;
}

bool viewport_has_presented_frame(ui::views::DrawHost* pane) {
  if (!pane ||
      pane->attach_mode() != ui::views::DrawHost::AttachMode::kContentMapView) {
    return false;
  }
  content::MapContents* session = pane->map_contents();
  if (!session || pane->view_id() == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session->HostView(pane->view_id());
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  // Software / DIB present may publish generation + size without a
  // DuplicateHandle NT section. FrameReady wire can also land before
  // SharedHandle — WaitFrameReady(0) covers that case.
  if (surface.generation > 0 && surface.width_px >= 8 &&
      surface.height_px >= 8) {
    return true;
  }
  return session->WaitFrameReady(pane->view_id(), 0);
}

}  // namespace detail
}  // namespace app
