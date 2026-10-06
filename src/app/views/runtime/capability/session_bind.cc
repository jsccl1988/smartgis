// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/capability/session_bind.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/harness/common/io/maps.h"
#include "app/views/harness/common/io/sample.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/common/pump/pump.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/view_host.h"
#include "gis/edit/memory_session.h"
#include "gis/style/document/style_document.h"
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

// SEH must not share a frame with C++ objects that need unwind.
void update_map_hwnd_seh(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  __try {
    UpdateWindow(hwnd);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

bool wait_map_ready(Browser& browser, int timeout_ms) {
  Browser* b = &browser;
  ui::views::DrawHost* map = b->draw_host();
  if (!map) {
    return false;
  }
  const DWORD budget = timeout_ms > 0 ? static_cast<DWORD>(timeout_ms) : 5000;
  const DWORD t0 = GetTickCount();
  for (;;) {
    content::Map2dPresenter* map2d = b->map2d();
    // SharedSurface FrameReady can fire on leftover GPU ocean/tessellation.
    // Wait until in-proc Map2dPresenter has built china carto layout.
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
  Browser* b = &browser;
  // Match run_tool_command: Scene3D / Data tabs own their ViewHost stacks.
  content::ViewHost* host = nullptr;
  if (ui::views::DrawHost* scene = b->scene_draw_host()) {
    if (scene->is_visible()) {
      host = b->scene_host();
    }
  }
  if (!host) {
    if (ui::views::DrawHost* data = b->data_draw_host()) {
      if (data->is_visible()) {
        host = b->data_host();
      }
    }
  }
  if (!host) {
    host = b->edit_view_host();
  }
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
  gis::FeatureGeom::Kind want = gis::FeatureGeom::Kind::kNone;
  if (kind == "point") {
    want = gis::FeatureGeom::Kind::kPoint;
  } else if (kind == "linestring" || kind == "line") {
    want = gis::FeatureGeom::Kind::kLineString;
  } else if (kind == "polygon" || kind == "poly") {
    want = gis::FeatureGeom::Kind::kPolygon;
  } else {
    return false;
  }
  const gis::FeatureMutation got = mem->committed_at(mem->committed_count() - 1);
  const int need = min_points > 0 ? min_points : 1;
  return got.op == gis::EditOp::kAppend && !got.geom.empty() &&
         got.geom.kind == want &&
         static_cast<int>(got.geom.points.size()) >= need;
}

bool browse_stress(Browser& browser, const wchar_t* leaf, int count) {
  Browser* b = &browser;
  content::ViewHost* host = b->edit_view_host();
  if (!host) {
    write_mark(leaf, "browse-stress-no-host", false);
    return false;
  }
  write_mark(leaf, "browse-stress-begin", false);
  // Do not KillTimer for the whole burst: ContentMapView + FORCE_GDI needs
  // WM_PAINT so HWND BitBlt / motion_gate see pan. Avoid PeekMessage of the
  // full UI queue (re-entrant AV); drive paints via UpdateWindow on the map
  // HWND only after each synthetic stroke.
  SetEnvironmentVariableA("skip-map-context-menu", "1");
  // Drop any leftover StretchBlt pan preview from OS-inject drag so FORCE_GDI
  // Map2d paint is the HWND SoT for motion_gate.
  if (b->blit()) {
    b->blit()->end_preview();
  }
  const int n = count > 0 ? count : 24;
  auto paint_map_hwnd = [b]() {
    ui::views::DrawHost* pane = b->draw_host();
    if (!pane) {
      return;
    }
    // Do not invalidate_frame_cache here — full china rebuild per stroke makes
    // PrintWindow/BitBlt burst drop below motion_gate frame counts.
    pane->invalidate_native();
    update_map_hwnd_seh(pane->native_view());
  };
  for (int i = 0; i < n; ++i) {
    if ((i % 8) == 0) {
      char step[32];
      std::snprintf(step, sizeof(step), "browse-stress-%d", i);
      write_mark(leaf, step, false);
    }
    // Large alternating pans so HWND client_bitblt / motion_gate center-crop
    // sees distinct frames (18px deltas were too small vs 96x54 gate crop).
    const int dir = (i & 1) ? -1 : 1;
    content::InputEvent pan_down{};
    pan_down.kind = content::InputEvent::Kind::kLDown;
    pan_down.x_px = 120 + (i % 5) * 10;
    pan_down.y_px = 80 + (i % 7) * 8;
    content::InputEvent pan_move = pan_down;
    pan_move.kind = content::InputEvent::Kind::kMouseMove;
    pan_move.x_px += dir * (64 + (i % 4) * 12);
    pan_move.y_px += dir * (40 + (i % 3) * 10);
    content::InputEvent pan_up = pan_move;
    pan_up.kind = content::InputEvent::Kind::kLUp;
    if (!host->dispatch_input(pan_down) || !host->dispatch_input(pan_move) ||
        !host->dispatch_input(pan_up)) {
      SetEnvironmentVariableA("skip-map-context-menu", nullptr);
      write_mark(leaf, "browse-stress-pan-fail", false);
      return false;
    }
    content::InputEvent wheel{};
    wheel.kind = content::InputEvent::Kind::kWheel;
    wheel.x_px = pan_move.x_px;
    wheel.y_px = pan_move.y_px;
    wheel.wheel = (i & 1) ? 120 : -120;
    if (!host->dispatch_input(wheel)) {
      SetEnvironmentVariableA("skip-map-context-menu", nullptr);
      write_mark(leaf, "browse-stress-wheel-fail", false);
      return false;
    }
    paint_map_hwnd();
    // Let the record burst (~8 fps client_bitblt) sample this pan state.
    ::Sleep(35);
  }
  ::Sleep(50);
  content::InputEvent rdown{};
  rdown.kind = content::InputEvent::Kind::kRDown;
  rdown.x_px = 50;
  rdown.y_px = 50;
  content::InputEvent rup = rdown;
  rup.kind = content::InputEvent::Kind::kRUp;
  // view.pan must not swallow RMB (shell owns the context menu).
  if (host->dispatch_input(rdown) || host->dispatch_input(rup)) {
    SetEnvironmentVariableA("skip-map-context-menu", nullptr);
    write_mark(leaf, "browse-stress-rmb-swallowed", false);
    return false;
  }
  SetEnvironmentVariableA("skip-map-context-menu", nullptr);
  write_mark(leaf, "browse-stress-end", false);
  return true;
}

bool expect_wheel_cursor(Browser& browser, const wchar_t* leaf, int x, int y) {
  Browser* b = &browser;
  content::ViewHost* host = b->edit_view_host();
  if (!host || !b->view_frame() || !b->orbit_frame()) {
    write_mark(leaf, "wheel-cursor-no-frame", false);
    return false;
  }
  const double scale0 = b->view_frame()->scale();
  content::InputEvent wheel{};
  wheel.kind = content::InputEvent::Kind::kWheel;
  wheel.x_px = x;
  wheel.y_px = y;
  wheel.wheel = -120;
  if (!host->dispatch_input(wheel)) {
    return false;
  }
  if (std::fabs(b->view_frame()->scale() - scale0) < 1e-9) {
    wheel.wheel = 120;
    if (!host->dispatch_input(wheel) ||
        std::fabs(b->view_frame()->scale() - scale0) < 1e-9) {
      return false;
    }
  }
  const render::rhi::CameraMatrices ortho =
      b->orbit_frame()->camera_matrices_ortho(800.f, 600.f);
  return ortho.kind == render::rhi::CameraKind::kOrtho;
}

bool apply_style_file(Browser& browser, const std::string& path_utf8) {
  content::MapScene* doc = browser.document();
  if (!doc || path_utf8.empty()) {
    return false;
  }
  std::ifstream in(path_utf8, std::ios::binary);
  if (!in) {
    return false;
  }
  std::string json((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  if (json.empty()) {
    return false;
  }
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(json, style.get())) {
    return false;
  }
  doc->set_style_document(std::move(style));
  return doc->style_document() != nullptr;
}

}  // namespace

void bind_session(Browser& browser,
                  content::CapabilityHost* out,
                  const wchar_t* mark_leaf) {
  Browser* b = &browser;
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : kUiShowcaseMarkLeaf;

  out->dispatch_edit_input = [b](const content::InputEvent& e) {
    content::ViewHost* host = b->edit_view_host();
    return host && host->dispatch_input(e);
  };
  out->wait_map_ready = [b](int timeout_ms) {
    return wait_map_ready(*b, timeout_ms);
  };
  out->load_china_sample = [b](bool write_stub) {
    return try_open_china_sample(*b, write_stub);
  };
  out->detach_maps = [b]() { detach_maps(*b); };
  out->stop_map_present_timers = [b]() { stop_map_present_timers(*b); };
  out->edit_host_ready = [b]() {
    content::ViewHost* host = b->edit_view_host();
    if (!host || !host->workspace() || !host->edits()) {
      return false;
    }
    return dynamic_cast<gis::MemoryEditSession*>(host->edits()) != nullptr;
  };
  out->run_tool = [b](const std::string& command_id) {
    return b->run_tool_command(command_id);
  };
  out->current_tool_id = [b]() { return current_tool_id(*b); };
  out->expect_last_geom = [b](const std::string& kind, int min_points) {
    return expect_last_geom(*b, kind, min_points);
  };
  out->browse_stress = [b, leaf](int count) {
    return browse_stress(*b, leaf, count);
  };
  out->expect_wheel_cursor = [b, leaf](int x, int y) {
    return expect_wheel_cursor(*b, leaf, x, y);
  };
  out->doc_clear = [b]() {
    if (content::MapScene* doc = b->document()) {
      doc->clear();
      doc->clear_style_document();
    }
    return true;
  };
  out->fit_extent = [b]() {
    b->fit_map_extent();
    return true;
  };
  out->apply_style_file = [b](const std::string& path_utf8) {
    return apply_style_file(*b, path_utf8);
  };
  out->invalidate_map2d = [b]() {
    if (content::Map2dPresenter* map2d = b->map2d()) {
      map2d->invalidate_frame_cache();
    }
    return true;
  };
}

}  // namespace detail
}  // namespace app
