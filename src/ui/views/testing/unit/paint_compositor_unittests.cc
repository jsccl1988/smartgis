// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Paint commit, shell compositor, and vblank unit tests.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ui/gis/catalog/layer_tree.h"
#include "ui/gfx/color/color.h"
#include "ui/gfx/display/vblank_wait.h"
#include "ui/gfx/display_list/display_list.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;
using ui::gfx::paint_counters;
using ui::gfx::reset_paint_counters;

void test_paint_fingerprint_locked_scene() {
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 80, 40});
  auto btn = std::make_unique<Button>("OK");
  btn->set_bounds({8, 8, 64, 24});
  root->add_child(std::move(btn));
  const std::uint32_t a = paint_fingerprint(root.get(), 80, 40);
  const std::uint32_t b = paint_fingerprint(root.get(), 80, 40);
  expect(a != 0, "fingerprint non-zero");
  expect(a == b, "fingerprint stable");
  // Different size must not collide with the locked 80x40 scene (best-effort).
  const std::uint32_t c = paint_fingerprint(root.get(), 81, 40);
  expect(c != a, "fingerprint size-sensitive");
}

void test_hover_paint_skips_unrelated_views() {
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 200, 40});
  auto left = std::make_unique<PaintProbe>();
  auto right = std::make_unique<PaintProbe>();
  PaintProbe* a = left.get();
  PaintProbe* b = right.get();
  a->set_bounds({0, 0, 40, 40});
  b->set_bounds({140, 0, 40, 40});
  root->add_child(std::move(left));
  root->add_child(std::move(right));
  a->set_hovered(true);
  const Rect dirty = a->bounds();
  paint_tree(root.get(), 200, 40, &dirty);
  expect(a->self_paints == 1, "hovered view paints");
  expect(b->self_paints == 0, "unrelated view skipped");
}

void test_paint_commit_snapshot_isolation() {
  auto root = std::make_unique<PaintProbe>();
  root->set_bounds({0, 0, 80, 24});
  PaintCommit frame;
  expect(commit_view_tree(root.get(), Rect{0, 0, 80, 24}, 80, 24, 12,
                          ui::gfx::color_rgb(30, 30, 30), &frame),
         "commit_view_tree ok");
  expect(frame.generation != 0, "commit generation assigned");
  expect(!frame.display_list.empty(), "commit copied commands");
  expect(root->self_paints == 1, "commit records paint_self once");

  // Mutate the live View list; committed snapshot must keep prior commands.
  root->invalidate_commands();
  root->set_bounds({0, 0, 10, 10});
  ui::gfx::DisplayList live;
  root->append_commands_to(&live);
  expect(root->self_paints == 2, "re-record after invalidate");
  expect(!frame.display_list.empty(), "committed list survives mutation");

  ui::gfx::DisplayList clone = frame.display_list.clone();
  expect(!clone.empty(), "DisplayList::clone keeps commands");
  frame.display_list.clear();
  expect(!clone.empty(), "clone independent of source clear");
}

void test_paint_commit_dirty_culls_commands() {
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 200, 200});
  auto first = std::make_unique<Button>("A");
  auto second = std::make_unique<Button>("B");
  first->set_bounds({0, 0, 40, 20});
  second->set_bounds({150, 150, 40, 20});
  const Rect first_bounds = first->bounds();
  root->add_child(std::move(first));
  root->add_child(std::move(second));

  PaintCommit full;
  expect(commit_view_tree(root.get(), Rect{0, 0, 200, 200}, 200, 200, 12,
                          ui::gfx::color_rgb(30, 30, 30), &full),
         "full-client commit ok");
  const size_t full_cmds = full.display_list.cmd_count();
  expect(full_cmds > 0, "full commit has commands");

  PaintCommit partial;
  expect(commit_view_tree(root.get(), first_bounds, 200, 200, 12,
                          ui::gfx::color_rgb(30, 30, 30), &partial),
         "dirty-rect commit ok");
  expect(partial.display_list.cmd_count() < full_cmds,
         "dirty cull emits fewer commands than full client");
}

namespace {

const wchar_t kShellWakeClass[] = L"SmartGisViewsShellWakeTest";
int g_shell_wake_posts = 0;

LRESULT CALLBACK shell_wake_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam) {
  if (msg == kShellPublishedMessage) {
    ++g_shell_wake_posts;
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

void test_shell_compositor_async_publish_wake() {
  // Focused: Commit does not require UI wait_published; worker posts
  // kShellPublishedMessage when the generation is front.
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = shell_wake_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kShellWakeClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  expect(registered, "shell wake test class registered");
  if (!registered) {
    return;
  }

  HWND hwnd =
      CreateWindowExW(0, kShellWakeClass, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE,
                      nullptr, GetModuleHandleW(nullptr), nullptr);
  expect(hwnd != nullptr, "shell wake message-only hwnd");
  if (!hwnd) {
    return;
  }

  g_shell_wake_posts = 0;
  ShellCompositor compositor;
  compositor.start();

  PaintCommit frame;
  frame.width_px = 16;
  frame.height_px = 16;
  frame.dirty = Rect{0, 0, 16, 16};
  frame.font_px = 12;
  frame.clear_color = ui::gfx::color_rgb(40, 40, 40);
  frame.generation = 7;
  compositor.notify_when_published(frame.generation, hwnd);
  compositor.commit(std::move(frame));

  // Product path must not block; this test only syncs to observe the wake.
  expect(compositor.wait_published(7), "async publish completes");
  expect(compositor.published_generation() == 7, "published generation 7");

  for (int i = 0; i < 200 && g_shell_wake_posts == 0; ++i) {
    MSG msg = {};
    while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (g_shell_wake_posts == 0) {
      Sleep(1);
    }
  }
  expect(g_shell_wake_posts >= 1, "worker posted kShellPublishedMessage");

  compositor.shutdown();
  DestroyWindow(hwnd);
}

// Resize/move leaves the client larger than the last published DIB. present()
// BitBlts the front first, then fills only uncovered margins (NULL_BRUSH +
// WM_ERASEBKGND=1 otherwise shows desktop). A full front cover must not
// FillRect the paint rect (that flash is mouse-move flicker).
void test_shell_compositor_present_fills_when_buffer_lags() {
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = 32;
  bmi.bmiHeader.biHeight = -32;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  expect(dib != nullptr && bits != nullptr, "present test dest DIB");
  if (!dib || !bits) {
    return;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  expect(mem != nullptr, "present test mem DC");
  if (!mem) {
    DeleteObject(dib);
    return;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  auto* px = static_cast<std::uint32_t*>(bits);
  for (int i = 0; i < 32 * 32; ++i) {
    px[i] = 0x00FF00FFu;  // poison magenta (BI_RGB BGRA)
  }

  const ui::gfx::Color fill = ui::gfx::color_rgb(55, 66, 77);
  ShellCompositor compositor;
  compositor.start();
  RECT dest = {0, 0, 32, 32};
  expect(compositor.present(mem, dest, fill) == 0, "empty present gen 0");
  // BI_RGB little-endian: B,G,R,(A/unused)
  const std::uint32_t fill_bgra =
      (77u) | (66u << 8) | (55u << 16);
  expect((px[0] & 0x00FFFFFFu) == fill_bgra, "empty present fills corner");
  expect((px[16 * 32 + 16] & 0x00FFFFFFu) == fill_bgra,
         "empty present fills center");

  PaintCommit frame;
  frame.width_px = 8;
  frame.height_px = 8;
  frame.dirty = Rect{0, 0, 8, 8};
  frame.font_px = 12;
  frame.clear_color = ui::gfx::color_rgb(1, 2, 3);
  frame.generation = 3;
  compositor.commit(std::move(frame));
  expect(compositor.wait_published(3), "small frame published");

  for (int i = 0; i < 32 * 32; ++i) {
    px[i] = 0x00FF00FFu;
  }
  expect(compositor.present(mem, dest, fill) == 3, "lag present gen 3");
  const std::uint32_t small_bgra = (3u) | (2u << 8) | (1u << 16);
  expect((px[2 * 32 + 2] & 0x00FFFFFFu) == small_bgra,
         "lag present copies front pixels");
  expect((px[20 * 32 + 20] & 0x00FFFFFFu) == fill_bgra,
         "lag present fills outside front");

  compositor.shutdown();
  SelectObject(mem, old);
  DeleteDC(mem);
  DeleteObject(dib);
}

  // Full-size published front must BitBlt without a prior FillRect wipe -- that
// flash was the mouse-hover flicker / hollow chrome symptom.
void test_shell_compositor_present_no_flash_when_front_covers() {
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = 16;
  bmi.bmiHeader.biHeight = -16;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  expect(dib != nullptr && bits != nullptr, "no-flash dest DIB");
  if (!dib || !bits) {
    return;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  expect(mem != nullptr, "no-flash mem DC");
  if (!mem) {
    DeleteObject(dib);
    return;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  auto* px = static_cast<std::uint32_t*>(bits);

  ShellCompositor compositor;
  compositor.start();
  PaintCommit frame;
  frame.width_px = 16;
  frame.height_px = 16;
  frame.dirty = Rect{0, 0, 16, 16};
  frame.font_px = 12;
  frame.clear_color = ui::gfx::color_rgb(10, 20, 30);
  frame.generation = 7;
  compositor.commit(std::move(frame));
  expect(compositor.wait_published(7), "cover frame published");

  const ui::gfx::Color poison_fill = ui::gfx::color_rgb(200, 10, 10);
  for (int i = 0; i < 16 * 16; ++i) {
    px[i] = 0x00DEADBEu;
  }
  RECT dest = {0, 0, 16, 16};
  expect(compositor.present(mem, dest, poison_fill) == 7, "cover present gen");
  const std::uint32_t front_bgra = (30u) | (20u << 8) | (10u << 16);
  const std::uint32_t poison_bgra = (10u) | (10u << 8) | (200u << 16);
  expect((px[0] & 0x00FFFFFFu) == front_bgra, "cover present uses front");
  expect((px[8 * 16 + 8] & 0x00FFFFFFu) == front_bgra,
         "cover present center is front");
  expect((px[0] & 0x00FFFFFFu) != poison_bgra,
         "cover present did not leave fill flash");

  compositor.shutdown();
  SelectObject(mem, old);
  DeleteDC(mem);
  DeleteObject(dib);
}

void test_set_layers_layouts_once() {
  LayerTree tree;
  tree.set_bounds({0, 0, 220, 400});
  std::vector<LayerTree::LayerDesc> layers(6);
  for (int i = 0; i < 6; ++i) {
    layers[static_cast<size_t>(i)].id = "L" + std::to_string(i);
    layers[static_cast<size_t>(i)].name = layers[static_cast<size_t>(i)].id;
  }
  reset_paint_counters();
  tree.set_layers(layers);
  expect(tree.layer_count() == 6, "set_layers row count");
  expect(paint_counters().layout_count == 1, "set_layers layouts once");
}

void test_vblank_clock_wait_returns() {
  ui::gfx::VblankClock clock;
  clock.set_hwnd(nullptr);
  // Cold DXGI factory/output enum can exceed a frame; warm before timing.
  (void)clock.wait_next(16);
  const DWORD t0 = GetTickCount();
  (void)clock.wait_next(16);
  const DWORD t1 = GetTickCount();
  (void)clock.wait_next(16);
  const DWORD t2 = GetTickCount();
  // Two waits should each finish within ~3 frames even on a slow panel.
  expect(t1 - t0 < 200, "first WaitForVBlank/fallback returns promptly");
  expect(t2 - t1 < 200, "second WaitForVBlank/fallback returns promptly");
  reset_paint_counters();
  ui::gfx::note_begin_frame_qpc(1);
  ui::gfx::note_begin_frame_to_present_qpc(10);
  expect(paint_counters().begin_frame_count == 1, "begin_frame count");
  expect(paint_counters().begin_frame_to_present_qpc == 10,
         "begin_frame latency accumulates");
}

