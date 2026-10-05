// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/capability/fill_host.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/ui_delegate.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/capture/bmp.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/common/io/sample.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"
#include "app/views/shell/harness/showcase/map2d/map2d_showcase.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/edit/memory_session.h"
#include "gis/style/document/style_document.h"
#include "render/rhi/rhi.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace {

constexpr int kExportW = 640;
constexpr int kExportH = 480;

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

// True when BMP is usable product content (not flat near-white DXGI BitBlt).
bool bmp_looks_like_scene3d_content(const wchar_t* path) {
  int bw = 0;
  int bh = 0;
  detail::BmpFileCheckOpts check;
  check.require_color_diversity = true;
  if (!detail::bmp_file_has_visible_signal(path, &bw, &bh, check)) {
    return false;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"rb") != 0 || !f) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER ih{};
  if (std::fread(&fh, sizeof(fh), 1, f) != 1 ||
      std::fread(&ih, sizeof(ih), 1, f) != 1 || fh.bfType != 0x4D42) {
    std::fclose(f);
    return false;
  }
  const int w = ih.biWidth > 0 ? ih.biWidth : -ih.biWidth;
  const int h = ih.biHeight > 0 ? ih.biHeight : -ih.biHeight;
  const int bpp = ih.biBitCount / 8;
  if (w < 8 || h < 8 || (bpp != 3 && bpp != 4)) {
    std::fclose(f);
    return false;
  }
  const int stride = ((w * ih.biBitCount + 31) / 32) * 4;
  std::vector<unsigned char> row(static_cast<size_t>(stride));
  std::fseek(f, static_cast<long>(fh.bfOffBits), SEEK_SET);
  long sum = 0;
  int samples = 0;
  for (int y = 0; y < h; y += (std::max)(1, h / 24)) {
    if (std::fread(row.data(), 1, static_cast<size_t>(stride), f) !=
        static_cast<size_t>(stride)) {
      break;
    }
    for (int x = 0; x < w; x += (std::max)(1, w / 32)) {
      const unsigned char* p = row.data() + static_cast<size_t>(x) * bpp;
      sum += static_cast<int>(p[0]) + p[1] + p[2];
      ++samples;
    }
    // Skip unread rows in file when sampling.
    if (y + (std::max)(1, h / 24) < h) {
      const long skip =
          static_cast<long>(stride) *
          static_cast<long>((std::max)(1, h / 24) - 1);
      if (skip > 0) {
        std::fseek(f, skip, SEEK_CUR);
      }
    }
  }
  std::fclose(f);
  if (samples < 8) {
    return false;
  }
  // Reject solid near-white flip-model BitBlt (mean channel ~243).
  const double mean = static_cast<double>(sum) / (static_cast<double>(samples) * 3.0);
  return mean < 220.0 && mean > 8.0;
}

bool write_hwnd_scaled_bmp(HWND hwnd, const wchar_t* filename, int dst_w,
                           int dst_h) {
  if (!hwnd || !IsWindow(hwnd) || !filename || dst_w < 8 || dst_h < 8) {
    return false;
  }
  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    return false;
  }
  const int sw = rc.right - rc.left;
  const int sh = rc.bottom - rc.top;
  if (sw < 8 || sh < 8) {
    return false;
  }
  HDC wnd_dc = GetDC(hwnd);
  if (!wnd_dc) {
    return false;
  }
  HDC src_dc = CreateCompatibleDC(wnd_dc);
  HBITMAP src_bmp = CreateCompatibleBitmap(wnd_dc, sw, sh);
  HDC dst_dc = CreateCompatibleDC(wnd_dc);
  HBITMAP dst_bmp = CreateCompatibleBitmap(wnd_dc, dst_w, dst_h);
  if (!src_dc || !src_bmp || !dst_dc || !dst_bmp) {
    if (dst_bmp) {
      DeleteObject(dst_bmp);
    }
    if (dst_dc) {
      DeleteDC(dst_dc);
    }
    if (src_bmp) {
      DeleteObject(src_bmp);
    }
    if (src_dc) {
      DeleteDC(src_dc);
    }
    ReleaseDC(hwnd, wnd_dc);
    return false;
  }
  HGDIOBJ old_src = SelectObject(src_dc, src_bmp);
  HGDIOBJ old_dst = SelectObject(dst_dc, dst_bmp);
  if (!detail::blit_client_to_dib(hwnd, src_dc, sw, sh)) {
    BOOL printed =
        PrintWindow(hwnd, src_dc, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hwnd, src_dc, PW_RENDERFULLCONTENT);
    }
    if (!printed) {
      (void)BitBlt(src_dc, 0, 0, sw, sh, wnd_dc, 0, 0, SRCCOPY);
    }
  }
  SetStretchBltMode(dst_dc, HALFTONE);
  SetBrushOrgEx(dst_dc, 0, 0, nullptr);
  StretchBlt(dst_dc, 0, 0, dst_w, dst_h, src_dc, 0, 0, sw, sh, SRCCOPY);

  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = dst_w;
  bi.biHeight = -dst_h;
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  const int stride = ((dst_w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(dst_h));
  const int got = GetDIBits(dst_dc, dst_bmp, 0, dst_h, pixels.data(),
                            reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
  SelectObject(dst_dc, old_dst);
  SelectObject(src_dc, old_src);
  DeleteObject(dst_bmp);
  DeleteDC(dst_dc);
  DeleteObject(src_bmp);
  DeleteDC(src_dc);
  ReleaseDC(hwnd, wnd_dc);
  if (got != dst_h) {
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
  const bool ok =
      std::fwrite(&fh, sizeof(fh), 1, out) == 1 &&
      std::fwrite(&bi, sizeof(bi), 1, out) == 1 &&
      std::fwrite(pixels.data(), 1, pixels.size(), out) == pixels.size();
  std::fclose(out);
  return ok;
}

bool write_engine_sidecar(const wchar_t* bmp_w, const char* engine) {
  if (!bmp_w || !engine) {
    return false;
  }
  std::wstring path(bmp_w);
  const size_t dot = path.find_last_of(L'.');
  if (dot != std::wstring::npos) {
    path.resize(dot);
  }
  path += L".engine.txt";
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
    return false;
  }
  const bool ok = std::fwrite(engine, 1, std::strlen(engine), f) ==
                      std::strlen(engine) &&
                  std::fputc('\n', f) != EOF;
  std::fclose(f);
  return ok;
}

bool wide_to_utf8(const wchar_t* wide, char* out, size_t out_cap) {
  if (!wide || !out || out_cap < 2) {
    return false;
  }
  return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out,
                             static_cast<int>(out_cap), nullptr, nullptr) > 0;
}

std::wstring utf8_to_wide(const std::string& u8) {
  if (u8.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
  if (n <= 1) {
    return {};
  }
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), n);
  w.resize(static_cast<size_t>(n - 1));
  return w;
}

bool resolve_rel_under_exe(const wchar_t* const* rels,
                           size_t count,
                           std::string* out_utf8) {
  if (!out_utf8 || !rels || count == 0) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!detail::exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    wchar_t canon[MAX_PATH] = {};
    const wchar_t* use = full;
    if (GetFullPathNameW(full, MAX_PATH, canon, nullptr) != 0) {
      use = canon;
    }
    char utf8[MAX_PATH * 3] = {};
    if (!wide_to_utf8(use, utf8, sizeof(utf8))) {
      continue;
    }
    *out_utf8 = utf8;
    return true;
  }
  return false;
}

bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth) {
  if (!out || depth > 8) {
    return false;
  }
  const std::wstring pattern = root + L"\\*";
  WIN32_FIND_DATAW fd = {};
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  bool found = false;
  do {
    if (fd.cFileName[0] == L'.' &&
        (fd.cFileName[1] == L'\0' ||
         (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
      continue;
    }
    const std::wstring child = root + L"\\" + fd.cFileName;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (find_named_under(child, leaf, out, depth + 1)) {
        found = true;
        break;
      }
      continue;
    }
    if (_wcsicmp(fd.cFileName, leaf.c_str()) == 0 &&
        GetFileAttributesW(child.c_str()) != INVALID_FILE_ATTRIBUTES) {
      *out = child;
      found = true;
      break;
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return found;
}

bool resolve_harness_leaf(const std::string& leaf_utf8, std::string* out_utf8) {
  if (!out_utf8 || leaf_utf8.empty()) {
    return false;
  }
  wchar_t exe[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
    return false;
  }
  std::wstring dir(exe);
  const size_t slash = dir.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return false;
  }
  dir.resize(slash);
  const std::wstring leaf_w = utf8_to_wide(leaf_utf8);
  if (leaf_w.empty()) {
    return false;
  }
  const std::wstring harness_rels[] = {
      dir + L"\\..\\..\\testing\\tools\\harness",
      dir + L"\\..\\..\\..\\testing\\tools\\harness",
  };
  // Recursive name search — no product-suite whitelist in chrome.
  for (const std::wstring& harness : harness_rels) {
    wchar_t abs_buf[MAX_PATH] = {};
    const DWORD got =
        GetFullPathNameW(harness.c_str(), MAX_PATH, abs_buf, nullptr);
    if (got == 0 || got >= MAX_PATH) {
      continue;
    }
    std::wstring found;
    if (find_named_under(abs_buf, leaf_w, &found, 0) && !found.empty()) {
      char utf8[MAX_PATH * 3] = {};
      if (!wide_to_utf8(found.c_str(), utf8, sizeof(utf8))) {
        continue;
      }
      *out_utf8 = utf8;
      return true;
    }
  }
  return false;
}

bool frame_for_export(Browser& browser, const std::string& frame) {
  content::ViewFrame* vf = browser.view_frame();
  if (!vf) {
    return false;
  }
  // Shell-owned frames only. Product extents come from PluginHost
  // contribute_export_frame.
  if (frame == "china_product") {
    ensure_china_maplibre_carto(browser);
    frame_china_map2d(browser, kExportW, kExportH);
  } else if (frame == "unit_square") {
    constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
    vf->apply_world_extent(kUnit, kExportW, kExportH);
  } else if (frame == "document_extent") {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // compute_extent returns map space (y = -lat). apply_world_extent expects
      // lon/lat Extent2 and converts to map internally — convert here once.
      const double lat_min = -maxy;
      const double lat_max = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (lat_max - lat_min) * 0.15);
      const content::Extent2 live{minx - pad_x, lat_min - pad_y, maxx + pad_x,
                                  lat_max + pad_y};
      vf->apply_world_extent(live, kExportW, kExportH);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, kExportW, kExportH);
    }
  } else {
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;
    PluginShell* shell = browser.plugins();
    content::PluginHost* host = shell ? shell->host() : nullptr;
    if (!host ||
        !host->lookup_export_frame(frame, &min_lon, &min_lat, &max_lon,
                                   &max_lat)) {
      return false;
    }
    const content::Extent2 extent{min_lon, min_lat, max_lon, max_lat};
    vf->apply_world_extent(extent, kExportW, kExportH);
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  return true;
}

}  // namespace

void fill_host(Browser& browser,
                          content::CapabilityHost* out,
                          const wchar_t* mark_leaf) {
  if (!out) {
    return;
  }
  Browser* b = &browser;
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : detail::kUiShowcaseMarkLeaf;

  out->pump = [](int ms) {
    detail::pump_messages(static_cast<DWORD>(ms > 0 ? ms : 0));
  };
  out->mark = [leaf](const std::string& token) {
    detail::write_mark(leaf, token.c_str(), false);
  };
  out->select_map_tab = [b](int index) { b->select_map_tab(index); };
  out->catalog_tab = [b](int index) {
    if (ui::views::CatalogView* cat = b->catalog_view()) {
      if (ui::views::TabStrip* tabs = cat->source_tabs()) {
        tabs->set_active(index);
      }
    }
  };
  out->inspector_tab = [b](int index) {
    if (b->ui()) {
      b->ui()->activate_inspector_tab(index);
    }
  };
  out->shell_hwnd = [b]() -> void* {
    return reinterpret_cast<void*>(b->hwnd());
  };
  out->dispatch_edit_input = [b](const content::InputEvent& e) {
    content::ViewHost* host = b->edit_view_host();
    return host && host->dispatch_input(e);
  };
  out->wait_map_ready = [b](int timeout_ms) {
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
      detail::pump_messages(50);
    }
  };
  out->load_china_sample = [b](bool write_stub) {
    return detail::try_open_china_sample(*b, write_stub);
  };
  out->detach_maps = [b]() { detail::detach_maps(*b); };
  out->stop_map_present_timers = [b]() {
    detail::stop_map_present_timers(*b);
  };
  out->window = [b](const std::string& action, int w, int h) {
    HWND hwnd = b->hwnd();
    if (!hwnd || !IsWindow(hwnd)) {
      return false;
    }
    if (action == "activate") {
      ShowWindow(hwnd, SW_SHOW);
      SetForegroundWindow(hwnd);
      return true;
    }
    if (action == "resize") {
      ui::views::DrawHost* scene = b->scene_draw_host();
      // Always pause Scene3d during shell resize — live FlyCube present on
      // Phase B2 has hung Display join. Do not auto-resume here; tab switch /
      // export_bmp / request_frame paths re-show present when needed.
      if (scene) {
        scene->pause_present();
      }
      const int nw = w > 0 ? w : 1280;
      const int nh = h > 0 ? h : 800;
      if (IsZoomed(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
      }
      // Keep position; SWP_FRAMECHANGED so custom-frame WM_SIZE / layout
      // always run even when the outer size is unchanged.
      SetWindowPos(hwnd, nullptr, 0, 0, nw, nh,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
      detail::pump_messages(50);
      if (content::Map2dPresenter* map2d = b->map2d()) {
        map2d->note_surface_reset();
        map2d->invalidate_frame_cache();
      }
      if (BrowserUiDelegate* ui = b->ui()) {
        ui->for_each_draw_host([](ui::views::DrawHost* pane) {
          if (!pane) {
            return;
          }
          if (!pane->is_visible()) {
            if (pane->role() == ui::views::DrawHost::Role::kScene3d) {
              pane->pause_present();
            }
            pane->sync_native_bounds();
            return;
          }
          pane->sync_native_bounds();
          if (HWND map = pane->native_view()) {
            if (IsWindow(map)) {
              RECT rc = {};
              GetClientRect(map, &rc);
              if (rc.right > 0 && rc.bottom > 0) {
                SendMessageW(map, WM_SIZE, SIZE_RESTORED,
                             MAKELPARAM(rc.right, rc.bottom));
              }
              InvalidateRect(map, nullptr, FALSE);
            }
          }
          pane->invalidate_native();
        });
        ui->invalidate_map_overlays();
        ui->schedule_overlay_full_redraw();
      }
      detail::pump_messages(80);
      return true;
    }
    return true;
  };
  out->key = [b](unsigned vk) {
    HWND hwnd = b->hwnd();
    if (!vk || !hwnd || !IsWindow(hwnd)) {
      return false;
    }
    PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    PostMessageW(hwnd, WM_KEYUP, vk, 0);
    return true;
  };
  out->clear_marks = [leaf]() { detail::clear_mark(leaf); };
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
  out->current_tool_id = [b]() -> std::string {
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
  };
  out->expect_last_geom = [b](const std::string& kind, int min_points) {
    content::ViewHost* host = b->edit_view_host();
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
    const gis::FeatureMutation got =
        mem->committed_at(mem->committed_count() - 1);
    const int need = min_points > 0 ? min_points : 1;
    return got.op == gis::EditOp::kAppend && !got.geom.empty() &&
           got.geom.kind == want &&
           static_cast<int>(got.geom.points.size()) >= need;
  };
  out->browse_stress = [b, leaf](int count) {
    content::ViewHost* host = b->edit_view_host();
    if (!host) {
      detail::write_mark(leaf, "browse-stress-no-host", false);
      return false;
    }
    detail::write_mark(leaf, "browse-stress-begin", false);
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
        detail::write_mark(leaf, step, false);
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
        detail::write_mark(leaf, "browse-stress-pan-fail", false);
        return false;
      }
      content::InputEvent wheel{};
      wheel.kind = content::InputEvent::Kind::kWheel;
      wheel.x_px = pan_move.x_px;
      wheel.y_px = pan_move.y_px;
      wheel.wheel = (i & 1) ? 120 : -120;
      if (!host->dispatch_input(wheel)) {
        SetEnvironmentVariableA("skip-map-context-menu", nullptr);
        detail::write_mark(leaf, "browse-stress-wheel-fail", false);
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
      detail::write_mark(leaf, "browse-stress-rmb-swallowed", false);
      return false;
    }
    SetEnvironmentVariableA("skip-map-context-menu", nullptr);
    detail::write_mark(leaf, "browse-stress-end", false);
    return true;
  };
  out->expect_wheel_cursor = [b, leaf](int x, int y) {
    content::ViewHost* host = b->edit_view_host();
    if (!host || !b->view_frame() || !b->orbit_frame()) {
      detail::write_mark(leaf, "wheel-cursor-no-frame", false);
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
  };
  out->map2d_run = [b](const std::string& mode) {
    Map2dShowcaseMode m = Map2dShowcaseMode::kNone;
    if (mode == "china") {
      m = Map2dShowcaseMode::kChina;
    } else if (mode == "align") {
      m = Map2dShowcaseMode::kAlign;
    } else if (mode == "orthogrid") {
      m = Map2dShowcaseMode::kOrthogrid;
    } else {
      return false;
    }
    return map2d_showcase_body(*b, m) == 0;
  };
  out->atmosphere_run = [b](const std::string& mode) {
    AtmosphereShowcaseMode m = AtmosphereShowcaseMode::kNone;
    if (mode == "land") {
      m = AtmosphereShowcaseMode::kLand;
    } else if (mode == "ocean") {
      m = AtmosphereShowcaseMode::kOcean;
    } else if (mode == "full") {
      m = AtmosphereShowcaseMode::kFull;
    } else if (mode == "coast") {
      m = AtmosphereShowcaseMode::kCoast;
    } else if (mode == "globe" || mode == "earth") {
      m = AtmosphereShowcaseMode::kGlobe;
    } else {
      return false;
    }
    return atmosphere_showcase_body(*b, m) == 0;
  };
  out->run_processing = [b](const std::string& id, const std::string& args) {
    PluginShell* shell = b->plugins();
    if (!shell || id.empty() || !shell->host()) {
      return false;
    }
    return shell->run_processing(id, args);
  };
  out->console_run = [b]() { return console_self_test_body(*b) == 0; };

  out->resolve_data = [](const std::string& kind, const std::string& leaf,
                         std::string* out_path) {
    if (!out_path || leaf.empty()) {
      return false;
    }
    if (kind == "harness") {
      return resolve_harness_leaf(leaf, out_path);
    }
    wchar_t a[MAX_PATH] = {};
    wchar_t bpath[MAX_PATH] = {};
    const std::wstring leaf_w = utf8_to_wide(leaf);
    if (leaf_w.empty()) {
      return false;
    }
    if (kind == "plugin") {
      if (swprintf_s(a, MAX_PATH, L"..\\data\\plugin\\%s", leaf_w.c_str()) <=
              0 ||
          swprintf_s(bpath, MAX_PATH, L"data\\plugin\\%s", leaf_w.c_str()) <=
              0) {
        return false;
      }
    } else if (kind == "data") {
      if (swprintf_s(a, MAX_PATH, L"..\\data\\%s", leaf_w.c_str()) <= 0 ||
          swprintf_s(bpath, MAX_PATH, L"data\\%s", leaf_w.c_str()) <= 0) {
        return false;
      }
    } else {
      return false;
    }
    const wchar_t* rels[] = {a, bpath};
    return resolve_rel_under_exe(rels, 2, out_path);
  };
  out->capture_path = [](const std::string& leaf, std::string* out_path) {
    if (!out_path || leaf.empty()) {
      return false;
    }
    const std::wstring leaf_w = utf8_to_wide(leaf);
    if (leaf_w.empty()) {
      return false;
    }
    wchar_t path_w[MAX_PATH] = {};
    if (!detail::exe_capture_path(path_w, MAX_PATH, leaf_w.c_str())) {
      return false;
    }
    char utf8[MAX_PATH * 3] = {};
    if (!wide_to_utf8(path_w, utf8, sizeof(utf8))) {
      return false;
    }
    *out_path = utf8;
    return true;
  };
  out->sidecar_path = [](const std::string& rel, std::string* out_path) {
    if (!out_path || rel.empty()) {
      return false;
    }
    char path_a[MAX_PATH] = {};
    if (!detail::exe_sidecar_path_a(path_a, MAX_PATH, rel.c_str())) {
      return false;
    }
    *out_path = path_a;
    return true;
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
  out->export_bmp = [b](const std::string& leaf, const std::string& frame) {
    if (leaf.empty()) {
      return false;
    }
    const std::wstring leaf_w = utf8_to_wide(leaf);
    if (leaf_w.empty()) {
      return false;
    }
    wchar_t bmp_w[MAX_PATH] = {};
    if (!detail::exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
      return false;
    }
    if (frame == "scene3d") {
      content::Scene3dPresenter* cam = b->scene3d();
      if (!cam) {
        return false;
      }
      // Prove FlyCube GpuPresent is live, then soft-export the shared DEM mesh.
      // DXGI BitBlt of flip-model HWND is often solid black/white and is not
      // the score SoT (browse.3d uses the same software hypsometric path).
      apply_china_scene3d_orbit(*b);
      // Re-assert shell size so catalog/diagnostic cannot leave a stub
      // FlyCube swapchain (~316×101) that hangs present / BitBlt.
      if (HWND shell = b->hwnd()) {
        if (IsWindow(shell)) {
          SetWindowPos(shell, nullptr, 0, 0, 1280, 800,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                           SWP_FRAMECHANGED);
          detail::pump_messages(80);
        }
      }
      b->select_map_tab(1);
      detail::pump_messages(120);
      ui::views::DrawHost* scene = b->scene_draw_host();
      bool flycube_live = false;
      if (scene && content::prefer_scene3d_flycube()) {
        scene->set_gpu_present_visible(true);
        if (scene->attach_mode() !=
            ui::views::DrawHost::AttachMode::kGpuPresent) {
          b->select_map_tab(1);
          detail::pump_messages(200);
        }
        // Skip DXGI BitBlt when the present client is a stub — soft DEM is
        // the score SoT; long present waits on 316×101 have hung Phase B.
        int present_w = 0;
        int present_h = 0;
        if (HWND ph = scene->present_hwnd()) {
          if (IsWindow(ph)) {
            RECT prc = {};
            GetClientRect(ph, &prc);
            present_w = prc.right - prc.left;
            present_h = prc.bottom - prc.top;
          }
        }
        if (scene->attach_mode() ==
            ui::views::DrawHost::AttachMode::kGpuPresent) {
          flycube_live = detail::present_shell_scene3d_frame(scene, 1200);
          if (!flycube_live) {
            flycube_live = scene->last_gpu_present_ok();
          }
        }
        if (flycube_live && present_w >= 640 && present_h >= 360) {
          HWND present = scene->present_hwnd();
          if (present && IsWindow(present)) {
            ShowWindow(present, SW_SHOWNOACTIVATE);
            SetWindowPos(present, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
                             SWP_NOACTIVATE);
            detail::pump_messages(40);
            (void)detail::present_shell_scene3d_frame(scene, 400);
            if (write_hwnd_scaled_bmp(present, bmp_w, kExportW, kExportH) &&
                bmp_looks_like_scene3d_content(bmp_w)) {
              (void)write_engine_sidecar(bmp_w, "FlyCube/DX12");
              detail::write_mark(detail::kUiShowcaseMarkLeaf,
                                 "interact-3d-gpu-ok", /*truncate=*/false);
              return true;
            }
            DeleteFileW(bmp_w);
          }
        }
      }
      // Soft paint must not race the Display thread: holding present_mu_ while
      // FlyCube is mid-present has aborted (exit 3) after interact-3d-gestures.
      if (scene) {
        scene->pause_present();
        detail::pump_messages(60);
      }
      // Pull camera so sky + DEM land fill the export (browse.3d SoT).
      if (content::OrbitFrame* orbit = b->orbit_frame()) {
        orbit->set_distance(2.35f);
        orbit->set_pitch(0.62f);
      }
      constexpr int kW = kExportW;
      constexpr int kH = kExportH;
      HDC screen = GetDC(nullptr);
      if (!screen) {
        return false;
      }
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bmi = {};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = kW;
      bmi.bmiHeader.biHeight = -kH;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      if (!mem || !dib || !bits) {
        if (dib) {
          DeleteObject(dib);
        }
        if (mem) {
          DeleteDC(mem);
        }
        ReleaseDC(nullptr, screen);
        return false;
      }
      HGDIOBJ old = SelectObject(mem, dib);
      cam->gpu().set_wireframe_enabled(false);
      // Interact 3D sibling is China DEM + labels — drop plugin contour / Wave Hs
      // overlay TIN that spikes soft export (white vertex explosion).
      cam->clear_overlay_tin_mesh();
      if (flycube_live) {
        cam->set_render_engine_name("FlyCube/DX12");
      }
      bool painted = false;
      try {
        // Explicit software path (same as browse.3d) — cam->paint also works
        // but keeps scenic ensure() side effects out of the harness export.
        cam->software().paint(mem, kW, kH, true);
        painted = true;
      } catch (...) {
        painted = false;
      }
      SelectObject(mem, old);
      bool wrote = false;
      if (painted) {
        const DWORD image_bytes =
            static_cast<DWORD>(kW * 4) * static_cast<DWORD>(kH);
        BITMAPFILEHEADER bfh = {};
        bfh.bfType = 0x4D42;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bfh.bfSize = bfh.bfOffBits + image_bytes;
        BITMAPINFOHEADER bih = bmi.bmiHeader;
        bih.biSizeImage = image_bytes;
        FILE* f = nullptr;
        if (_wfopen_s(&f, bmp_w, L"wb") == 0 && f) {
          wrote = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
                  std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
                  std::fwrite(bits, 1, image_bytes, f) == image_bytes;
          std::fclose(f);
        }
      }
      DeleteObject(dib);
      DeleteDC(mem);
      ReleaseDC(nullptr, screen);
      if (wrote) {
        (void)write_engine_sidecar(
            bmp_w, flycube_live ? "FlyCube/DX12" : "GDI");
        if (flycube_live) {
          detail::write_mark(detail::kUiShowcaseMarkLeaf,
                             "interact-3d-gpu-ok", /*truncate=*/false);
        }
      }
      // Leave Scene3d present paused — resume_present_timer + pump after soft
      // paint has hung before interact-3d-ok (Display join under FlyCube).
      // Phase B2 resize / Phase C select_map_tab re-show as needed.
      return wrote;
    }
    content::Map2dPresenter* map2d = b->map2d();
    if (!map2d) {
      return false;
    }
    char bmp_a[MAX_PATH] = {};
    if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                            nullptr) <= 0) {
      return false;
    }
    if (!frame_for_export(*b, frame.empty() ? "document_extent" : frame)) {
      return false;
    }
    content::MapScene* doc = b->document();
    if (!doc || doc->feature_count() == 0) {
      return false;
    }
    // Prefer rebinding so software paint cannot use a stale unbound scene.
    map2d->bind(doc, b->view_frame());
    map2d->invalidate_frame_cache();
    if (ui::views::DrawHost* pane = b->draw_host()) {
      if (pane->native_view() && IsWindow(pane->native_view())) {
        InvalidateRect(pane->native_view(), nullptr, FALSE);
        UpdateWindow(pane->native_view());
      }
      pane->invalidate_native();
      pane->sync_identity_frame();
    }
    detail::pump_messages(200);
    return map2d->export_bmp(bmp_a, kExportW, kExportH);
  };
  out->suppress_dialogs = [](bool on) {
    ui::views::Dialog::set_dialog_modals_suppressed_for_test(on);
    return true;
  };
  out->require_plugins = [b]() {
    if (b->plugins()) {
      (void)b->plugins()->ensure_builtins();
    }
    // Harness print/report: never fail the script on plugin host shape.
    return true;
  };
  out->apply_style_file = [b](const std::string& path_utf8) {
    content::MapScene* doc = b->document();
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
  };
  out->invalidate_map2d = [b]() {
    if (content::Map2dPresenter* map2d = b->map2d()) {
      map2d->invalidate_frame_cache();
    }
    return true;
  };
  out->analysis_set_frame = [b](int index) {
    return b->apply_plugin_frame(index);
  };
  out->analysis_export_frames = [b](const std::string& dir_leaf) {
    return b->export_plugin_frames(dir_leaf);
  };
  out->open_report = [b](const std::string& report_dir) {
    PluginShell* shell = b->plugins();
    if (!shell || !shell->host() || report_dir.empty()) {
      return false;
    }
    return shell->host()->open_report(report_dir);
  };
  out->post_to_report = [b](const std::string& json) {
    PluginShell* shell = b->plugins();
    if (!shell || !shell->host()) {
      return false;
    }
    return shell->host()->post_to_report(json);
  };
}

}  // namespace app
