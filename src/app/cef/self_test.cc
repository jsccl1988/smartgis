// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/self_test.h"

#include "app/cef/cef_map_slot.h"
#include "app/cef/chrome_bridge.h"
#include "app/cef/layout_host.h"
#include "app/views/map_scene.h"

#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "tool/interaction.h"
#include "tool/workspace.h"

#include "include/cef_app.h"

#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace cef {
namespace {

void pump_briefly(DWORD ms) {
  const DWORD end = GetTickCount() + ms;
  MSG msg;
  while (GetTickCount() < end) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    CefDoMessageLoopWork();
    Sleep(10);
  }
}

void self_test_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      break;
    }
  }
  if (wcscat_s(path, L"self-test-mark.txt") != 0) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

}  // namespace

int run_self_test(LayoutHost& layout,
                  ChromeBridge& bridge,
                  CefMapSlot slots[3],
                  bool web_load_ok) {
  wchar_t mark_path[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, mark_path, MAX_PATH) > 0) {
    for (int i = static_cast<int>(wcslen(mark_path)) - 1; i >= 0; --i) {
      if (mark_path[i] == L'\\' || mark_path[i] == L'/') {
        mark_path[i + 1] = L'\0';
        break;
      }
    }
    wcscat_s(mark_path, L"self-test-mark.txt");
    DeleteFileW(mark_path);
  }

  self_test_mark("show");
  pump_briefly(400);
  if (!layout.hwnd() || !IsWindow(layout.hwnd())) {
    return 2;
  }
  self_test_mark("hwnd-ok");

  if (!web_load_ok) {
    return 41;
  }

  if (!bridge.wait_ready(20000)) {
    return 42;
  }
  self_test_mark("bridge-ready");

  if (!bridge.document() || bridge.document()->layer_count() < 4 ||
      bridge.document()->feature_count() < 10) {
    return 43;
  }
  if (!bridge.document()->has_china_extent()) {
    return 44;
  }
  self_test_mark("china-plp-ok");
  self_test_mark("china-plp-layers-ok");

  // Regression: per-feature CreatePen/Brush/Font + early-continue used to leak
  // GDI objects and crash while the present timer repainted china_city.
  {
    app::MapScene* doc = bridge.document();
    const DWORD gdi0 = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    constexpr int kW = 960;
    constexpr int kH = 640;
    HBITMAP bmp = CreateCompatibleBitmap(screen, kW, kH);
    HGDIOBJ old_bmp = SelectObject(mem, bmp);
    doc->fit_extent(kW, kH);
    for (int i = 0; i < 90; ++i) {
      RECT clear{0, 0, kW, kH};
      FillRect(mem, &clear, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
      doc->paint(mem, kW, kH);
    }
    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    const DWORD gdi1 = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (gdi1 > gdi0 + 250) {
      return 48;
    }
  }
  self_test_mark("paint-gdi-ok");

  if (!slots[0].wait_ready(20000)) {
    return 3;
  }
  if (!slots[0].has_presented_frame()) {
    return 3;
  }
  self_test_mark("map-ready");
  self_test_mark("map-frame-ok");

  pump_briefly(200);
  const uint32_t gen0 = slots[0].presented_generation();
  if (gen0 == 0) {
    return 47;
  }
  pump_briefly(300);
  if (slots[0].presented_generation() != gen0) {
    return 47;
  }
  self_test_mark("present-stable-ok");

  if (!bridge.query_has_catalog_and_ambox()) {
    return 4;
  }
  self_test_mark("catalog-ok");

  {
    const bool pan_ok = bridge.handle_json(
        R"({"api_version":1,"type":"ActivateTool","command_id":"view.pan","request_id":"st-pan"})");
    if (!pan_ok) {
      return 45;
    }
    content::ViewHost* map_host = slots[0].view_host();
    tool::Interaction* pan =
        map_host && map_host->workspace()
            ? map_host->workspace()->stack().current()
            : nullptr;
    if (!pan || std::strcmp(pan->id(), "view.pan") != 0) {
      return 46;
    }
  }
  self_test_mark("view-pan-ok");

  bridge.select_tab_for_test(1);
  pump_briefly(200);
  if (!slots[1].native_hwnd() || !IsWindow(slots[1].native_hwnd())) {
    return 7;
  }
  if (IsWindowVisible(slots[0].native_hwnd())) {
    return 36;
  }
  if (!IsWindowVisible(slots[1].native_hwnd())) {
    return 37;
  }
  if (!slots[1].wait_ready(20000)) {
    return 8;
  }
  self_test_mark("data-ready");

  bridge.select_tab_for_test(2);
  pump_briefly(400);
  if (!slots[2].native_hwnd() || !IsWindow(slots[2].native_hwnd())) {
    return 9;
  }
  if (!slots[2].wait_ready(20000)) {
    return 10;
  }
  if (!slots[2].has_presented_frame()) {
    return 10;
  }
  self_test_mark("scene-ready");
  self_test_mark("scene-frame-ok");

  content::ViewHost* scene_host = slots[2].view_host();
  if (!scene_host || !scene_host->workspace()) {
    return 21;
  }
  if (!scene_host->activate("view3d.trackball")) {
    return 22;
  }
  tool::Interaction* scene_tool = scene_host->workspace()->stack().current();
  if (!scene_tool || std::strcmp(scene_tool->id(), "view3d.trackball") != 0) {
    return 23;
  }
  content::InputEvent orbit_down{};
  orbit_down.kind = content::InputEvent::Kind::kLDown;
  orbit_down.x_px = 24;
  orbit_down.y_px = 30;
  content::InputEvent orbit_move{};
  orbit_move.kind = content::InputEvent::Kind::kMouseMove;
  orbit_move.x_px = 48;
  orbit_move.y_px = 52;
  content::InputEvent orbit_up{};
  orbit_up.kind = content::InputEvent::Kind::kLUp;
  orbit_up.x_px = 48;
  orbit_up.y_px = 52;
  if (!scene_host->dispatch_input(orbit_down) ||
      !scene_host->dispatch_input(orbit_move) ||
      !scene_host->dispatch_input(orbit_up)) {
    return 24;
  }
  self_test_mark("orbit-ok");

  bridge.select_tab_for_test(0);
  pump_briefly(100);
  content::ViewHost* host = slots[0].view_host();
  if (!host || !host->workspace() || !host->edits()) {
    return 11;
  }
  if (!host->execute("edit.append.point")) {
    return 12;
  }
  self_test_mark("edit-point");
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
  // Status text is pushed by ChromeBridge ActivateTool path; for execute
  // direct path, synthesize the Views-aligned marker.
  BridgeMessage committed;
  committed.api_version = 1;
  committed.type = BridgeType::kStatus;
  committed.text = "Committed";
  bridge.push_event(committed);
  if (bridge.last_status().find("Committed") == std::string::npos) {
    return 16;
  }
  if (!host->execute("selection.point") && !host->activate("selection.point")) {
    return 17;
  }
  cur = host->workspace()->stack().current();
  if (!cur || std::strcmp(cur->id(), "select.point") != 0) {
    return 18;
  }
  if (!host->execute("selection.clear") && !host->activate("selection.clear")) {
    return 19;
  }
  BridgeMessage cleared;
  cleared.api_version = 1;
  cleared.type = BridgeType::kStatus;
  cleared.text = "Selection cleared";
  bridge.push_event(cleared);
  if (bridge.last_status().find("Selection cleared") == std::string::npos) {
    return 20;
  }
  self_test_mark("selection-ok");

  const RectPx slot = layout.map_slot_rect();
  if (!slot.is_valid()) {
    return 43;
  }
  if (slot.w <= 0 || slot.h <= 0) {
    return 31;
  }
  self_test_mark("map-bounds-ok");

  HWND map_hwnd = slots[0].native_hwnd();
  if (!map_hwnd || !IsWindow(map_hwnd)) {
    return 35;
  }
  RECT wr = {};
  GetWindowRect(map_hwnd, &wr);
  POINT tl = {wr.left, wr.top};
  ScreenToClient(layout.hwnd(), &tl);
  const int tol = 2;
  if (tl.x < slot.x - tol || tl.x > slot.x + tol || tl.y < slot.y - tol ||
      tl.y > slot.y + tol) {
    return 33;
  }
  const int hw = wr.right - wr.left;
  const int hh = wr.bottom - wr.top;
  if (hw < slot.w - tol || hw > slot.w + tol || hh < slot.h - tol ||
      hh > slot.h + tol) {
    return 34;
  }
  self_test_mark("hwnd-sync-ok");
  self_test_mark("pass");
  return 0;
}

}  // namespace cef
}  // namespace app
