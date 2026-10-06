// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/probe.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/pump.h"
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

struct GeomToken {
  std::string_view token;
  gis::FeatureGeom::Kind kind;
};

constexpr GeomToken kGeomTokens[] = {
    {"point", gis::FeatureGeom::Kind::kPoint},
    {"linestring", gis::FeatureGeom::Kind::kLineString},
    {"line", gis::FeatureGeom::Kind::kLineString},
    {"polygon", gis::FeatureGeom::Kind::kPolygon},
    {"poly", gis::FeatureGeom::Kind::kPolygon},
};

gis::FeatureGeom::Kind geom_kind_from_token(std::string_view token) {
  for (const GeomToken& row : kGeomTokens) {
    if (row.token == token) {
      return row.kind;
    }
  }
  return gis::FeatureGeom::Kind::kNone;
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

struct FaceWait {
  bool scene = false;
  int missing = 3;
  int timeout = 3;
};

FaceWait face_wait(std::string_view face) {
  if (face == "scene") {
    return FaceWait{.scene = true, .missing = 9, .timeout = 10};
  }
  return {};
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

bool edit_host_ready(Browser& browser) {
  content::ViewHost* host = browser.edit_view_host();
  if (!host || !host->workspace() || !host->edits()) {
    return false;
  }
  return dynamic_cast<gis::MemoryEditSession*>(host->edits()) != nullptr;
}

bool wait_map_ready(Browser& browser, int timeout_ms) {
  ui::views::DrawHost* map = browser.draw_host();
  if (!map) {
    return false;
  }
  const DWORD budget = timeout_ms > 0 ? static_cast<DWORD>(timeout_ms) : 5000;
  const DWORD t0 = GetTickCount();
  for (;;) {
    content::Map2dPresenter* map2d = browser.map2d();
    if (map2d && map2d->layout_build_count() > 0) {
      return true;
    }
    if (GetTickCount() - t0 >= budget) {
      return map->wait_ready(1) && map2d && map2d->layout_build_count() > 0;
    }
    pump_messages(50);
  }
}

std::string current_tool_id(Browser& browser) {
  content::ViewHost* host = tool_stack_host(browser);
  if (!host || !host->workspace()) {
    return {};
  }
  tool::Interaction* cur = host->workspace()->stack().current();
  if (!cur || !cur->id()) {
    return {};
  }
  return std::string(cur->id());
}

bool expect_last_geom(Browser& browser,
                      const std::string& kind,
                      int min_points) {
  content::ViewHost* host = browser.edit_view_host();
  if (!host || !host->edits()) {
    return false;
  }
  auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
  if (!mem || mem->committed_count() < 1) {
    return false;
  }
  const gis::FeatureGeom::Kind want = geom_kind_from_token(kind);
  if (want == gis::FeatureGeom::Kind::kNone) {
    return false;
  }
  const gis::FeatureMutation got = mem->committed_at(mem->committed_count() - 1);
  const int need = min_points > 0 ? min_points : 1;
  return got.op == gis::EditOp::kAppend && !got.geom.empty() &&
         got.geom.kind == want &&
         static_cast<int>(got.geom.points.size()) >= need;
}

bool expect_wheel_cursor(Browser& browser, const wchar_t* leaf, int x, int y) {
  content::ViewHost* host = browser.edit_view_host();
  if (!host || !browser.view_frame() || !browser.orbit_frame()) {
    write_mark(leaf, "wheel-cursor-no-frame", false);
    return false;
  }
  const double scale0 = browser.view_frame()->scale();
  content::InputEvent wheel{};
  wheel.kind = content::InputEvent::Kind::kWheel;
  wheel.x_px = x;
  wheel.y_px = y;
  wheel.wheel = -120;
  if (!host->dispatch_input(wheel)) {
    return false;
  }
  if (std::fabs(browser.view_frame()->scale() - scale0) < 1e-9) {
    wheel.wheel = 120;
    if (!host->dispatch_input(wheel) ||
        std::fabs(browser.view_frame()->scale() - scale0) < 1e-9) {
      return false;
    }
  }
  const render::rhi::CameraMatrices ortho =
      browser.orbit_frame()->camera_matrices_ortho(800.f, 600.f);
  return ortho.kind == render::rhi::CameraKind::kOrtho;
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
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}

int wait_content_viewport(Browser& browser,
                          const std::string& face,
                          int timeout_ms,
                          bool want_frame) {
  const FaceWait slot = face_wait(face);
  ui::views::DrawHost* pane =
      slot.scene ? browser.scene_draw_host() : browser.draw_host();
  if (!pane) {
    return slot.missing;
  }
  if (pane->attach_mode() != ui::views::DrawHost::AttachMode::kContentMapView) {
    return 0;
  }
  if (slot.scene) {
    pane->sync_native_bounds();
    pane->invalidate_native();
  }
  const uint32_t budget =
      timeout_ms > 0 ? static_cast<uint32_t>(timeout_ms) : 90000u;
  if (!pane->wait_ready(budget)) {
    return slot.timeout;
  }
  if (want_frame && !viewport_has_presented_frame(pane)) {
    return slot.timeout;
  }
  return 0;
}

}  // namespace detail
}  // namespace app
