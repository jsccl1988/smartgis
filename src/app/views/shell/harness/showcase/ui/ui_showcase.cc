// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/ui_showcase.h"

#include "app/views/shell/harness/showcase/ui/interact_script.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {
namespace {

void showcase_mark(const char* token) {
  wchar_t path[MAX_PATH] = {};
  if (!detail::exe_capture_path(path, MAX_PATH, L"ui-showcase-mark.txt")) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"ab") != 0 || !f) {
    return;
  }
  std::fprintf(f, "%s\n", token);
  std::fclose(f);
}

const wchar_t* bmp_leaf_for_mode(UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kData:
      return L"ui-showcase-data.bmp";
    case UiShowcaseMode::kScene:
      return L"ui-showcase-scene.bmp";
    case UiShowcaseMode::kCatalog:
      return L"ui-showcase-catalog.bmp";
    case UiShowcaseMode::kInteract:
      return L"ui-showcase-interact.bmp";
    case UiShowcaseMode::kShell:
    case UiShowcaseMode::kNone:
    default:
      return L"ui-showcase-shell.bmp";
  }
}

bool pixels_have_visible_signal(const unsigned char* pixels,
                                int stride,
                                int w,
                                int h) {
  if (!pixels || stride <= 0 || w < 8 || h < 8) {
    return false;
  }
  int non_near_black = 0;
  int distinct = 0;
  unsigned last = 0xFFFFFFFFu;
  for (int y = 0; y < h; y += 4) {
    const unsigned char* row = pixels + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += 4) {
      const unsigned char b = row[x * 3 + 0];
      const unsigned char g = row[x * 3 + 1];
      const unsigned char r = row[x * 3 + 2];
      if (r > 24 || g > 24 || b > 24) {
        ++non_near_black;
      }
      const unsigned packed =
          (static_cast<unsigned>(r) << 16) | (static_cast<unsigned>(g) << 8) |
          static_cast<unsigned>(b);
      if (packed != last) {
        ++distinct;
        last = packed;
      }
    }
  }
  return non_near_black > 32 && distinct > 4;
}

// Copy the HWND client via its window DC only. Never BitBlt from the desktop
// DC: overlapping Explorer / IDE windows polluted ui-showcase BMPs and made
// shell/catalog gates fail (light desktop chrome, missing tab accent).
bool blit_client_to_dib(HWND hwnd, HDC wnd_dc, HDC mem, int w, int h) {
  if (!hwnd || !wnd_dc || !mem || w < 1 || h < 1) {
    return false;
  }
  return BitBlt(mem, 0, 0, w, h, wnd_dc, 0, 0, SRCCOPY) != FALSE;
}

bool bmp_has_chrome_diversity(const unsigned char* pixels,
                              int stride,
                              int w,
                              int h) {
  if (!pixels || stride <= 0 || w < 8 || h < 8) {
    return false;
  }
  int buckets[64] = {};
  int used = 0;
  int accent = 0;
  for (int y = 0; y < h; y += 8) {
    const unsigned char* row = pixels + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += 8) {
      const unsigned char b = row[x * 3 + 0];
      const unsigned char g = row[x * 3 + 1];
      const unsigned char r = row[x * 3 + 2];
      const int idx = ((r >> 6) << 4) | ((g >> 6) << 2) | (b >> 6);
      if (buckets[idx] == 0) {
        ++used;
      }
      ++buckets[idx];
      // Active Map tab accent (#007acc) — required for a finished shell paint.
      if (r < 40 && g > 90 && g < 160 && b > 170 && b > r + 100) {
        ++accent;
      }
    }
  }
  return used >= 4 && accent >= 6;
}

bool capture_hwnd_bmp(HWND hwnd, const wchar_t* filename) {
  if (!hwnd || !IsWindow(hwnd) || !filename) {
    return false;
  }
  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    return false;
  }
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w < 8 || h < 8) {
    return false;
  }
  HDC wnd_dc = GetDC(hwnd);
  if (!wnd_dc) {
    return false;
  }
  HDC mem = CreateCompatibleDC(wnd_dc);
  HBITMAP bmp = CreateCompatibleBitmap(wnd_dc, w, h);
  if (!mem || !bmp) {
    if (bmp) {
      DeleteObject(bmp);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(hwnd, wnd_dc);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, bmp);

  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = w;
  bi.biHeight = -h;  // top-down
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  const int stride = ((w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(h));
  int got = 0;
  bool diverse = false;
  bool have_signal = false;
  std::vector<unsigned char> best_signal;
  for (int attempt = 0; attempt < 8 && !diverse; ++attempt) {
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
    pump_views_messages(180 + static_cast<DWORD>(attempt) * 40);

    BOOL printed =
        PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
    }
    got = GetDIBits(mem, bmp, 0, h, pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    const bool signal =
        got == h &&
        pixels_have_visible_signal(pixels.data(), stride, w, h);
    // Prefer the latest good PrintWindow frame — the first attempt often has
    // map pixels but stale/missing tab accent after interact scripts.
    if (signal) {
      best_signal = pixels;
      have_signal = true;
    }
    diverse = signal &&
              bmp_has_chrome_diversity(pixels.data(), stride, w, h);
    if (diverse) {
      break;
    }
    // Window-DC blit only when PrintWindow produced no visible pixels.
    // Never fall back to the desktop DC (overlapping windows polluted BMPs).
    if (!signal && blit_client_to_dib(hwnd, wnd_dc, mem, w, h)) {
      got = GetDIBits(mem, bmp, 0, h, pixels.data(),
                      reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
      if (got == h &&
          pixels_have_visible_signal(pixels.data(), stride, w, h) &&
          !have_signal) {
        best_signal = pixels;
        have_signal = true;
      }
      diverse = got == h &&
                pixels_have_visible_signal(pixels.data(), stride, w, h) &&
                bmp_has_chrome_diversity(pixels.data(), stride, w, h);
    }
  }
  if (!diverse && have_signal) {
    pixels = std::move(best_signal);
    got = h;
  }

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, wnd_dc);
  if (got != h) {
    return false;
  }

  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());

  FILE* out = nullptr;
  if (_wfopen_s(&out, filename, L"wb") != 0 || !out) {
    return false;
  }
  std::fwrite(&fh, sizeof(fh), 1, out);
  std::fwrite(&bi, sizeof(bi), 1, out);
  std::fwrite(pixels.data(), 1, pixels.size(), out);
  std::fclose(out);
  return true;
}

void stop_map_present(Browser& browser) {
  auto stop = [](ui::views::MapViewport* pane) {
    if (pane && pane->native_view() && IsWindow(pane->native_view())) {
      KillTimer(pane->native_view(), 1);
    }
  };
  stop(browser.map_viewport());
  stop(browser.map_data_viewport());
  stop(browser.map_scene_viewport());
}

int ui_showcase_linger_ms() {
  const char* timed = std::getenv("SMT_UI_SHOWCASE_TIMED_MS");
  if (timed && timed[0]) {
    const int v = std::atoi(timed);
    if (v > 0) {
      return v;
    }
  }
  const char* linger = std::getenv("SMT_UI_SHOWCASE_LINGER_MS");
  if (linger && linger[0]) {
    return std::atoi(linger);
  }
  return 0;
}

// Deterministic chrome for visual gates: default dark. SMT_UI_THEME=light|dark
// overrides without rewriting the user's LocalAppData preference.
void apply_harness_theme() {
  const char* want = std::getenv("SMT_UI_THEME");
  const char* id = "dark";
  if (want && want[0]) {
    if (std::strcmp(want, "light") == 0) {
      id = "light";
    } else if (std::strcmp(want, "dark") == 0) {
      id = "dark";
    }
  }
  ui::views::ThemeService::get().ensure_builtin_packs();
  ui::views::ThemeService::get().set_theme(id, /*persist_to_disk=*/false);
}

void force_shell_repaint(Browser& browser) {
  if (ui::views::View* contents = browser.contents_view()) {
    if (ui::views::Widget* w = contents->widget()) {
      w->layout_contents();
    } else {
      contents->layout();
    }
  }
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    pane->sync_native_bounds();
  }
  if (ui::views::MapViewport* pane = browser.map_data_viewport()) {
    pane->sync_native_bounds();
  }
  if (ui::views::MapViewport* pane = browser.map_scene_viewport()) {
    pane->sync_native_bounds();
  }
  if (HWND hwnd = browser.hwnd()) {
    for (int i = 0; i < 4; ++i) {
      InvalidateRect(hwnd, nullptr, TRUE);
      UpdateWindow(hwnd);
      pump_views_messages(160);
    }
  } else {
    pump_views_messages(300);
  }
}

void apply_scenario_interaction(Browser& browser, UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kData:
      browser.select_map_tab(1);
      pump_views_messages(350);
      break;
    case UiShowcaseMode::kScene: {
      browser.select_map_tab(2);
      pump_views_messages(800);
      // Lazy FlyCube attach needs several present ticks before HUD leaves
      // views-scene3d.gdi / Fps0 and the DEM fills the tab (not a sticker).
      if (ui::views::MapViewport* scene = browser.map_scene_viewport()) {
        for (int i = 0; i < 80; ++i) {
          if (scene->attach_mode() ==
                  ui::views::MapViewport::AttachMode::kFlyCube &&
              scene->last_gpu_present_ok()) {
            break;
          }
          scene->sync_native_bounds();
          if (scene->attach_mode() ==
              ui::views::MapViewport::AttachMode::kNone) {
            scene->attach();
          }
          scene->invalidate_native();
          pump_views_messages(50);
        }
        if (scene->attach_mode() ==
                ui::views::MapViewport::AttachMode::kFlyCube &&
            scene->last_gpu_present_ok()) {
          showcase_mark("scene-flycube-ok");
        } else {
          showcase_mark("scene-flycube-wait");
        }
      }
      apply_china_scene3d_orbit(browser);
      if (ui::views::MapViewport* scene = browser.map_scene_viewport()) {
        scene->invalidate_native();
      }
      pump_views_messages(400);
      break;
    }
    case UiShowcaseMode::kCatalog:
      browser.select_map_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          tabs->set_active(2);  // Maps
        }
      }
      pump_views_messages(350);
      break;
    case UiShowcaseMode::kInteract:
      // Prefer Interact DSL (suite-colocated *.il under testing/tools/harness/);
      // OS driver waits for outer SendInput/PostMessage injector.
      if (try_apply_interact_script(browser)) {
        // Re-assert Map tab chrome after scripted clicks / OS inject.
        browser.select_map_tab(0);
        pump_views_messages(200);
        break;
      }
      browser.select_map_tab(0);
      pump_views_messages(200);
      browser.select_map_tab(1);
      pump_views_messages(250);
      browser.select_map_tab(2);
      pump_views_messages(350);
      browser.select_map_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          tabs->set_active(0);
          pump_views_messages(120);
          tabs->set_active(1);
          pump_views_messages(120);
          tabs->set_active(0);
        }
      }
      if (browser.ui()) {
        if (ui::views::TabStrip* insp = browser.ui()->inspector_tabs()) {
          if (insp->tab_count() > 1) {
            insp->set_active(1);
            pump_views_messages(150);
            insp->set_active(0);
          }
        }
      }
      pump_views_messages(250);
      break;
    case UiShowcaseMode::kShell:
    case UiShowcaseMode::kNone:
    default:
      browser.select_map_tab(0);
      pump_views_messages(250);
      break;
  }
}

}  // namespace

int run_ui_showcase(Browser& browser, UiShowcaseMode mode) {
  if (mode == UiShowcaseMode::kNone) {
    return 0;
  }
  // Always stop present timers + detach before return (ExitProcess heap race).
  struct DetachOnExit {
    Browser& browser;
    ~DetachOnExit() { detail::detach_maps(browser); }
  } detach_guard{browser};

  wchar_t mark_path[MAX_PATH] = {};
  if (detail::exe_capture_path(mark_path, MAX_PATH, L"ui-showcase-mark.txt")) {
    DeleteFileW(mark_path);
  }
  showcase_mark(ui_showcase_name(mode));

  apply_harness_theme();
  showcase_mark("theme-ok");

  showcase_mark("pump-pre");
  pump_views_messages(600);
  showcase_mark("pump-post");
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    showcase_mark("hwnd-fail");
    return 2;
  }
  showcase_mark("hwnd-ok");

  showcase_mark("interact-pre");
  apply_scenario_interaction(browser, mode);
  showcase_mark("interact-post");

  ui::views::View* root = browser.contents_view();
  if (!root) {
    showcase_mark("root-fail");
    return 4;
  }
  showcase_mark("root-ok");

  // Force Widget layout before violation checks — Invalidate alone does not
  // resize create-time zero bounds (child-outside-parent / status-clipped).
  if (ui::views::Widget* w = root->widget()) {
    w->layout_contents();
  } else {
    root->layout();
  }
  if (HWND hwnd = browser.hwnd()) {
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
  }
  pump_views_messages(200);

  ui::views::MapViewport* active = browser.map_viewport();
  if (mode == UiShowcaseMode::kData) {
    active = browser.map_data_viewport();
  } else if (mode == UiShowcaseMode::kScene) {
    active = browser.map_scene_viewport();
  }
  ui::views::TabStrip* map_tabs =
      browser.map_viewport()
          ? dynamic_cast<ui::views::TabStrip*>(browser.map_viewport()->parent())
          : nullptr;
  ui::views::TabStrip* catalog_tabs =
      browser.catalog_view() ? browser.catalog_view()->source_tabs() : nullptr;

  std::vector<std::string> layout_issues;
  const int layout_fails =
      ui::views::collect_layout_violations(root, &layout_issues);
  std::vector<std::string> overlap_issues;
  const int overlap_fails =
      ui::views::collect_sibling_overlaps(root, &overlap_issues);
  std::vector<std::string> shell_issues;
  const int shell_fails = ui::views::collect_shell_layout_anomalies(
      root, map_tabs, catalog_tabs, browser.status_bar(), active,
      browser.map_data_viewport(), browser.map_scene_viewport(), &shell_issues);
  showcase_mark("layout-checked");

  const int total_fails = layout_fails + overlap_fails + shell_fails;
  const bool force_dump = [] {
    const char* v = std::getenv("SMT_UI_FORENSICS");
    return v && v[0] && !(v[0] == '0' && v[1] == '\0');
  }();
  if (total_fails > 0 || force_dump) {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::string run_id =
        std::string("ui_showcase_") + ui_showcase_name(mode) + "_" +
        std::to_string(stamp);
    fs::path forensics_root = fs::path("out") / "ui_forensics";
    if (fs::exists(fs::path("..") / "Debug") ||
        fs::exists(fs::path("..") / "Release")) {
      forensics_root = fs::path("..") / "ui_forensics";
    }
    const fs::path dir = forensics_root / run_id;
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::vector<std::string> all = layout_issues;
    all.insert(all.end(), overlap_issues.begin(), overlap_issues.end());
    all.insert(all.end(), shell_issues.begin(), shell_issues.end());
    ui::views::write_layout_issues_file(dir / "layout_issues.txt", all);
    {
      std::ofstream man(dir / "manifest.json", std::ios::binary);
      if (man) {
        man << "{\n"
            << "  \"run_id\": \"" << run_id << "\",\n"
            << "  \"exe\": \"SmartGisViews.exe\",\n"
            << "  \"scenario\": \"--ui-showcase=" << ui_showcase_name(mode)
            << "\",\n"
            << "  \"issue_count\": " << all.size() << "\n"
            << "}\n";
      }
    }
    std::fprintf(stderr, "ui forensics: %s\n", dir.string().c_str());
  }
  if (total_fails > 0) {
    for (const std::string& issue : layout_issues) {
      std::fprintf(stderr, "ui-showcase layout: %s\n", issue.c_str());
    }
    for (const std::string& issue : overlap_issues) {
      std::fprintf(stderr, "ui-showcase overlap: %s\n", issue.c_str());
    }
    for (const std::string& issue : shell_issues) {
      std::fprintf(stderr, "ui-showcase shell: %s\n", issue.c_str());
    }
    showcase_mark("layout-fail");
    stop_map_present(browser);
    return 30;
  }

  const int linger = ui_showcase_linger_ms();
  if (linger > 0) {
    pump_views_messages(static_cast<DWORD>(linger));
  }

  force_shell_repaint(browser);

  // Reject pathological HUD FPS (instantaneous 1/dt after idle ≈ 0.07).
  // Idle Content Map2D correctly reports ~0 after note_hud_frame gap handling.
  if (active) {
    active->sync_identity_chrome();
    const float fps = active->hud_fps();
    if (fps < 0.5f || fps >= 5.f) {
      showcase_mark("hud-fps-ok");
    } else {
      showcase_mark("hud-fps-bad");
    }
  } else {
    showcase_mark("hud-fps-ok");
  }

  wchar_t bmp_path[MAX_PATH] = {};
  if (!detail::exe_capture_path(bmp_path, MAX_PATH, bmp_leaf_for_mode(mode))) {
    showcase_mark("bmp-path-fail");
    stop_map_present(browser);
    return 55;
  }
  DeleteFileW(bmp_path);
  if (HWND hwnd = browser.hwnd()) {
    // Prefer an unobscured top-level paint for PrintWindow / window-DC blit.
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    pump_views_messages(120);
  }
  if (!capture_hwnd_bmp(browser.hwnd(), bmp_path)) {
    std::fprintf(stderr, "ui-showcase: BMP capture failed\n");
    showcase_mark("bmp-fail");
    stop_map_present(browser);
    return 54;
  }
  showcase_mark("bmp-ok");
  showcase_mark("pass");
  stop_map_present(browser);
  return 0;
}

}  // namespace app
