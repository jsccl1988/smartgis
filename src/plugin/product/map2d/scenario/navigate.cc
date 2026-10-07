// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/probe.h"

#include "plugin/runtime/host/capability/shell.h"
#include <windows.h>
#include <shellapi.h>

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/app/content_main.h"
#include "content/embed/embed_sample.h"
#include "content/public/event_bus.h"
#include "content/public/map_contents.h"
#include "content/public/map_layer_types.h"
#include "content/public/view_host.h"
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

int scenario_navigate(HarnessShell& browser) {
  // C++ navigate stage for full --self-test. Lean suite "browse" is browse.il.

// Pan tool must activate without crash (Map tab).
if (!browser.run_tool_command("view.pan")) {
  browser.detach_maps();
  return 40;
}
{
  content::ViewHost* host = browser.edit_view_host();
  tool::Interaction* cur =
      host && host->workspace() ? host->workspace()->stack().current()
                                : nullptr;
  if (!cur || std::strcmp(cur->id(), "view.pan") != 0) {
    browser.detach_maps();
    return 41;
  }
  content::InputEvent pan_down{};
  pan_down.kind = content::InputEvent::Kind::kLDown;
  pan_down.x_px = 40;
  pan_down.y_px = 40;
  content::InputEvent pan_move{};
  pan_move.kind = content::InputEvent::Kind::kMouseMove;
  pan_move.x_px = 70;
  pan_move.y_px = 55;
  content::InputEvent pan_up{};
  pan_up.kind = content::InputEvent::Kind::kLUp;
  pan_up.x_px = 70;
  pan_up.y_px = 55;
  if (!host->dispatch_input(pan_down) || !host->dispatch_input(pan_move) ||
      !host->dispatch_input(pan_up)) {
    browser.detach_maps();
    return 42;
  }
  browser.mark("pan-ok");
}
// Browse stress: MapLibre-like pan/wheel bursts must not AV; RMB must not
// be swallowed by view.pan (shell owns the context menu).
{
  content::ViewHost* host = browser.edit_view_host();
  if (!host) {
    browser.detach_maps();
    return 49;
  }
  // Stop ALL map present timers while we burst-dispatch synthetic input.
  // Concurrent WM_TIMER present + pan/wheel has AVd under exe_smoke (exit
  // 0xC0000005 after pan-ok) when Data/3D HWNDs keep ticking after tab walks.
  browser.stop_present_timers();
  SetEnvironmentVariableA("skip-map-context-menu", "1");
  for (int i = 0; i < 24; ++i) {
    content::InputEvent pan_down{};
    pan_down.kind = content::InputEvent::Kind::kLDown;
    pan_down.x_px = 30 + (i % 5) * 8;
    pan_down.y_px = 30 + (i % 7) * 6;
    content::InputEvent pan_move = pan_down;
    pan_move.kind = content::InputEvent::Kind::kMouseMove;
    pan_move.x_px += 18;
    pan_move.y_px += 12;
    content::InputEvent pan_up = pan_move;
    pan_up.kind = content::InputEvent::Kind::kLUp;
    if (!host->dispatch_input(pan_down) ||
        !host->dispatch_input(pan_move) || !host->dispatch_input(pan_up)) {
      SetEnvironmentVariableA("skip-map-context-menu", nullptr);
      browser.detach_maps();
      return 49;
    }
    content::InputEvent wheel{};
    wheel.kind = content::InputEvent::Kind::kWheel;
    wheel.x_px = pan_move.x_px;
    wheel.y_px = pan_move.y_px;
    wheel.wheel = (i & 1) ? 120 : -120;
    if (!host->dispatch_input(wheel)) {
      SetEnvironmentVariableA("skip-map-context-menu", nullptr);
      browser.detach_maps();
      return 49;
    }
    // Sleep only â€?pumping WM_PAINT/present during the burst races input.
    ::Sleep(20);
  }
  ::Sleep(50);
  content::InputEvent rdown{};
  rdown.kind = content::InputEvent::Kind::kRDown;
  rdown.x_px = 50;
  rdown.y_px = 50;
  content::InputEvent rup = rdown;
  rup.kind = content::InputEvent::Kind::kRUp;
  if (host->dispatch_input(rdown) || host->dispatch_input(rup)) {
    SetEnvironmentVariableA("skip-map-context-menu", nullptr);
    browser.detach_maps();
    return 50;
  }
  SetEnvironmentVariableA("skip-map-context-menu", nullptr);
  browser.mark("browse-ok");
}
// Wheel-to-cursor must change overlay scale (not view-center zoom).
{
  content::ViewHost* host = browser.edit_view_host();
  if (!host) {
    browser.detach_maps();
    return 47;
  }
  const double scale0 = browser.view_frame()->scale();
  content::InputEvent wheel{};
  wheel.kind = content::InputEvent::Kind::kWheel;
  wheel.x_px = 40;
  wheel.y_px = 40;
  // Zoom out first: zoom-in can no-op if scale is already near the clamp.
  wheel.wheel = -120;
  if (!host->dispatch_input(wheel)) {
    std::fprintf(stderr, "wheel-cursor: dispatch_input failed\n");
    browser.detach_maps();
    return 47;
  }
  if (std::fabs(browser.view_frame()->scale() - scale0) < 1e-9) {
    // Retry zoom-in in case an observer reset the first delta.
    wheel.wheel = 120;
    if (!host->dispatch_input(wheel) ||
        std::fabs(browser.view_frame()->scale() - scale0) < 1e-9) {
      std::fprintf(stderr,
                   "wheel-cursor: scale unchanged (was %.9g now %.9g)\n",
                   scale0, browser.view_frame()->scale());
      browser.detach_maps();
      return 47;
    }
  }
  const render::rhi::CameraMatrices ortho =
      browser.orbit_frame()->camera_matrices_ortho(800.f, 600.f);
  if (ortho.kind != render::rhi::CameraKind::kOrtho) {
    browser.detach_maps();
    return 48;
  }
  browser.mark("wheel-cursor-ok");
}
  return 0;
}

}  // namespace plugin
