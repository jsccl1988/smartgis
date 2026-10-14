// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/atom/capture.h"

#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/il.runtime/backend/view/pixel/bmp.h"
#include "app/views/il.runtime/backend/view/pixel/gate.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <windows.h>

namespace app {
namespace detail {
namespace {

// Child client mapped into the shell DIB. Screen origin is the child's
// top-left in screen space (DXGI flip-model CAPTUREBLT source).
struct ChildPlace {
  int dst_x = 0;
  int dst_y = 0;
  int src_x = 0;
  int src_y = 0;
  int copy_w = 0;
  int copy_h = 0;
  int child_w = 0;
  int child_h = 0;
  POINT screen{0, 0};
};

bool place_child_in_shell(HWND shell,
                          HWND child,
                          int shell_w,
                          int shell_h,
                          int min_w,
                          int min_h,
                          ChildPlace* out) {
  if (!shell || !child || !out || !IsWindow(shell) || !IsWindow(child) ||
      shell_w < 8 || shell_h < 8) {
    return false;
  }
  RECT child_rc = {};
  if (!GetClientRect(child, &child_rc)) {
    return false;
  }
  const int cw = child_rc.right - child_rc.left;
  const int ch = child_rc.bottom - child_rc.top;
  if (cw < min_w || ch < min_h) {
    return false;
  }
  POINT origin = {0, 0};
  POINT shell_origin = {0, 0};
  if (!ClientToScreen(child, &origin) || !ClientToScreen(shell, &shell_origin)) {
    return false;
  }
  const int dst_x = origin.x - shell_origin.x;
  const int dst_y = origin.y - shell_origin.y;
  if (dst_x >= shell_w || dst_y >= shell_h) {
    return false;
  }
  const int copy_w = std::min(cw, shell_w - std::max(0, dst_x));
  const int copy_h = std::min(ch, shell_h - std::max(0, dst_y));
  if (copy_w < min_w || copy_h < min_h) {
    return false;
  }
  out->dst_x = dst_x;
  out->dst_y = dst_y;
  out->src_x = dst_x < 0 ? -dst_x : 0;
  out->src_y = dst_y < 0 ? -dst_y : 0;
  out->copy_w = copy_w;
  out->copy_h = copy_h;
  out->child_w = cw;
  out->child_h = ch;
  out->screen = origin;
  return true;
}

// Copy the HWND client via its window DC only. Never BitBlt from the desktop
// DC: overlapping Explorer / IDE windows polluted UI capture BMPs and made
// shell/catalog gates fail (light desktop horizon, missing tab accent).
bool blit_client_to_dib(HWND hwnd, HDC wnd_dc, HDC mem, int w, int h) {
  if (!hwnd || !wnd_dc || !mem || w < 1 || h < 1) {
    return false;
  }
  return BitBlt(mem, 0, 0, w, h, wnd_dc, 0, 0, SRCCOPY) != FALSE;
}

// PrintWindow of the shell often leaves the child DrawHost HWND as a dark
// hole. Blit the map client into the shell DIB at its client-relative origin.
bool composite_map_hwnd_into_shell(HWND shell,
                                   HWND map,
                                   HDC mem,
                                   int shell_w,
                                   int shell_h) {
  ChildPlace place;
  if (!mem || !place_child_in_shell(shell, map, shell_w, shell_h, 8, 8, &place)) {
    return false;
  }
  const int blit_x = std::max(0, place.dst_x);
  const int blit_y = std::max(0, place.dst_y);
  const bool dxgi_flip =
      (GetWindowLongPtrW(map, GWL_EXSTYLE) & WS_EX_NOREDIRECTIONBITMAP) != 0;
  // Flip-model DXGI (WS_EX_NOREDIRECTIONBITMAP) has no GDI redirection bitmap.
  // GetDC BitBlt returns the class BLACK_BRUSH and must not run first — a
  // failed CAPTUREBLT used to leave that poison in the shell DIB (ui.shell
  // hollow map: present frame_items cream, BMP center (0,0,0)).
  if (dxgi_flip) {
    if (!IsWindowVisible(map)) {
      ShowWindow(map, SW_SHOWNOACTIVATE);
    }
    // Match testing/tools/loop/record/capture.py: desktop SRCCOPY of the
    // TOPMOST FlyCube Present rect (no CAPTUREBLT). CAPTUREBLT has returned
    // solid black for flip-model popups while SRCCOPY sees DWM composition.
    SetWindowPos(map, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    RECT wr = {};
    if (!GetWindowRect(map, &wr)) {
      return false;
    }
    HDC screen = GetDC(nullptr);
    if (!screen) {
      return false;
    }
    const BOOL ok =
        BitBlt(mem, blit_x, blit_y, place.copy_w, place.copy_h, screen,
               wr.left + place.src_x, wr.top + place.src_y, SRCCOPY);
    ReleaseDC(nullptr, screen);
    return ok != FALSE;
  }
  HDC map_dc = GetDC(map);
  if (!map_dc) {
    return false;
  }
  const BOOL gdi_ok = BitBlt(mem, blit_x, blit_y, place.copy_w, place.copy_h,
                             map_dc, place.src_x, place.src_y, SRCCOPY);
  ReleaseDC(map, map_dc);
  return gdi_ok != FALSE;
}

bool composite_hud_hwnd_into_shell(HWND shell,
                                   HWND hud,
                                   HDC mem,
                                   int shell_w,
                                   int shell_h) {
  ChildPlace place;
  if (!mem || !place_child_in_shell(shell, hud, shell_w, shell_h, 8, 4, &place)) {
    return false;
  }
  HDC tmp = CreateCompatibleDC(mem);
  HBITMAP tmp_bmp =
      tmp ? CreateCompatibleBitmap(mem, place.child_w, place.child_h) : nullptr;
  HGDIOBJ old = tmp_bmp ? SelectObject(tmp, tmp_bmp) : nullptr;
  BOOL printed = FALSE;
  if (tmp && tmp_bmp) {
    printed = PrintWindow(hud, tmp, PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hud, tmp, PW_RENDERFULLCONTENT);
    }
    if (!printed) {
      HDC hdc = GetDC(hud);
      if (hdc) {
        printed = BitBlt(tmp, 0, 0, place.child_w, place.child_h, hdc, 0, 0,
                         SRCCOPY);
        ReleaseDC(hud, hdc);
      }
    }
    if (printed) {
      BitBlt(mem, std::max(0, place.dst_x), std::max(0, place.dst_y),
             place.copy_w, place.copy_h, tmp, place.src_x, place.src_y, SRCCOPY);
    }
  }
  if (tmp && old) {
    SelectObject(tmp, old);
  }
  if (tmp_bmp) {
    DeleteObject(tmp_bmp);
  }
  if (tmp) {
    DeleteDC(tmp);
  }
  return printed != FALSE;
}

// One PrintWindow / window-DC sample scored for visible pixels and shell
// diversity. |got| is the GetDIBits row count.
struct ShellSample {
  int got = 0;
  bool signal = false;
  bool diverse = false;
};

ShellSample sample_shell_dib(HDC mem,
                             HBITMAP bmp,
                             BITMAPINFOHEADER* bi,
                             unsigned char* pixels,
                             int stride,
                             int w,
                             int h) {
  ShellSample sample;
  sample.got = GetDIBits(mem, bmp, 0, static_cast<UINT>(h), pixels,
                         reinterpret_cast<BITMAPINFO*>(bi), DIB_RGB_COLORS);
  sample.signal =
      sample.got == h &&
      pixels_have_visible_signal(pixels, stride, w, h,
                                 VisiblePolicy::kSparseDistinct);
  sample.diverse =
      sample.signal && bmp_has_shell_diversity(pixels, stride, w, h);
  return sample;
}

void pin_topmost(HWND hwnd, bool on) {
  if (hwnd && IsWindow(hwnd)) {
    SetWindowPos(hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }
}

void release_shell_dib(HDC mem, HGDIOBJ old, HBITMAP bmp, HDC wnd_dc, HWND hwnd) {
  if (mem && old) {
    SelectObject(mem, old);
  }
  if (bmp) {
    DeleteObject(bmp);
  }
  if (mem) {
    DeleteDC(mem);
  }
  if (wnd_dc) {
    ReleaseDC(hwnd, wnd_dc);
  }
}

}  // namespace

bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd,
                          HWND hud_hwnd) {
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
    release_shell_dib(mem, nullptr, bmp, wnd_dc, hwnd);
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
  bool diverse = false;
  bool have_signal = false;
  std::vector<unsigned char> best_signal;
  const bool interact_leaf =
      wcsstr(filename, L"ui-showcase-interact") != nullptr;
  pin_topmost(hwnd, true);
  // Do not TOPMOST the map child for interact: it covers Skia horizon and
  // PrintWindow then writes a hollow shell (ui.interact inspect).
  if (!interact_leaf) {
    pin_topmost(map_hwnd, true);
  }
  pin_topmost(hud_hwnd, true);
  for (int attempt = 0; attempt < 8 && !diverse; ++attempt) {
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
    pump_views_messages(180 + static_cast<DWORD>(attempt) * 40);

    BOOL printed =
        PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
    }
    if (printed) {
      (void)composite_map_hwnd_into_shell(hwnd, map_hwnd, mem, w, h);
      (void)composite_hud_hwnd_into_shell(hwnd, hud_hwnd, mem, w, h);
    }
    ShellSample sample =
        sample_shell_dib(mem, bmp, &bi, pixels.data(), stride, w, h);
    // Prefer the latest good PrintWindow frame — the first attempt often has
    // map pixels but stale/missing tab accent after interact scripts.
    if (sample.signal) {
      best_signal = pixels;
      have_signal = true;
    }
    diverse = sample.diverse;
    if (diverse) {
      break;
    }
    // Window-DC blit only when PrintWindow produced no visible pixels.
    // Never fall back to the desktop DC (overlapping windows polluted BMPs).
    if (!sample.signal && blit_client_to_dib(hwnd, wnd_dc, mem, w, h)) {
      (void)composite_map_hwnd_into_shell(hwnd, map_hwnd, mem, w, h);
      (void)composite_hud_hwnd_into_shell(hwnd, hud_hwnd, mem, w, h);
      sample = sample_shell_dib(mem, bmp, &bi, pixels.data(), stride, w, h);
      if (sample.signal && !have_signal) {
        best_signal = pixels;
        have_signal = true;
      }
      diverse = sample.diverse;
    }
  }
  pin_topmost(hud_hwnd, false);
  pin_topmost(map_hwnd, false);
  pin_topmost(hwnd, false);
  int got = 0;
  if (!diverse && have_signal) {
    pixels = std::move(best_signal);
    got = h;
  } else if (diverse) {
    got = h;
  }

  release_shell_dib(mem, old, bmp, wnd_dc, hwnd);
  // Reject flat fills (e.g. scene PrintWindow under DXGI present) — writing
  // them as bmp-ok hid a blank ui.scene capture from the harness.
  const bool diverse_ok = bmp_has_shell_diversity(pixels.data(), stride, w, h);
  if (got != h ||
      !pixels_have_visible_signal(pixels.data(), stride, w, h,
                                 VisiblePolicy::kSparseDistinct) ||
      !diverse_ok) {
    return false;
  }

  return write_bmp_file(filename, bi, pixels.data(), pixels.size());
}

namespace {

bool load_bmp_file(const wchar_t* path,
                   BITMAPINFOHEADER* bi,
                   std::vector<unsigned char>* pixels) {
  if (!path || !bi || !pixels) {
    return false;
  }
  FILE* in = nullptr;
  if (_wfopen_s(&in, path, L"rb") != 0 || !in) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  if (std::fread(&fh, sizeof(fh), 1, in) != 1 ||
      std::fread(bi, sizeof(*bi), 1, in) != 1 || fh.bfType != 0x4D42) {
    std::fclose(in);
    return false;
  }
  const int w = bi->biWidth;
  const int h = bi->biHeight < 0 ? -bi->biHeight : bi->biHeight;
  if (w < 8 || h < 8 || w > kMaxBmpEdge || h > kMaxBmpEdge ||
      (bi->biBitCount != 24 && bi->biBitCount != 32)) {
    std::fclose(in);
    return false;
  }
  const int stride = ((w * bi->biBitCount + 31) / 32) * 4;
  pixels->assign(static_cast<size_t>(stride) * static_cast<size_t>(h), 0);
  if (std::fseek(in, static_cast<long>(fh.bfOffBits), SEEK_SET) != 0 ||
      std::fread(pixels->data(), 1, pixels->size(), in) != pixels->size()) {
    std::fclose(in);
    return false;
  }
  std::fclose(in);
  return true;
}

}  // namespace

bool stamp_map_export_into_shell_bmp(const wchar_t* shell_bmp,
                                     HWND shell,
                                     HWND map,
                                     const wchar_t* map_export_bmp) {
  ChildPlace place;
  RECT shell_rc = {};
  if (!shell_bmp || !map_export_bmp || !shell || !map ||
      !GetClientRect(shell, &shell_rc)) {
    return false;
  }
  const int shell_w = shell_rc.right - shell_rc.left;
  const int shell_h = shell_rc.bottom - shell_rc.top;
  if (!place_child_in_shell(shell, map, shell_w, shell_h, 8, 8, &place)) {
    return false;
  }
  BITMAPINFOHEADER shell_bi{};
  std::vector<unsigned char> shell_px;
  if (!load_bmp_file(shell_bmp, &shell_bi, &shell_px) ||
      shell_bi.biBitCount != 24) {
    return false;
  }
  const int sw = shell_bi.biWidth;
  const int sh =
      shell_bi.biHeight < 0 ? -shell_bi.biHeight : shell_bi.biHeight;
  const int shell_stride = ((sw * 24 + 31) / 32) * 4;
  const bool shell_top_down = shell_bi.biHeight < 0;
  // Always stamp: cream+line DXGI smear is "lit" enough to skip a near-black
  // gate, but is not carto SoT.
  BITMAPINFOHEADER map_bi{};
  std::vector<unsigned char> map_px;
  if (!load_bmp_file(map_export_bmp, &map_bi, &map_px)) {
    return false;
  }
  const int mw = map_bi.biWidth;
  const int mh = map_bi.biHeight < 0 ? -map_bi.biHeight : map_bi.biHeight;
  const int map_bpp = map_bi.biBitCount / 8;
  const int map_stride = ((mw * map_bi.biBitCount + 31) / 32) * 4;
  const bool map_top_down = map_bi.biHeight < 0;
  if (mw < 8 || mh < 8 || map_bpp < 3) {
    return false;
  }
  const int dst_x = std::max(0, place.dst_x);
  const int dst_y = std::max(0, place.dst_y);
  const int copy_w = std::min(place.copy_w, std::min(mw, sw - dst_x));
  const int copy_h = std::min(place.copy_h, std::min(mh, sh - dst_y));
  if (copy_w < 8 || copy_h < 8) {
    return false;
  }
  for (int y = 0; y < copy_h; ++y) {
    const int src_y = map_top_down ? y : (mh - 1 - y);
    const int dst_row_y = shell_top_down ? (dst_y + y) : (sh - 1 - (dst_y + y));
    if (src_y < 0 || src_y >= mh || dst_row_y < 0 || dst_row_y >= sh) {
      continue;
    }
    const unsigned char* src =
        map_px.data() + static_cast<size_t>(src_y) * map_stride +
        static_cast<size_t>(place.src_x) * static_cast<size_t>(map_bpp);
    unsigned char* dst =
        shell_px.data() + static_cast<size_t>(dst_row_y) * shell_stride +
        static_cast<size_t>(dst_x) * 3u;
    for (int x = 0; x < copy_w; ++x) {
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
      src += map_bpp;
      dst += 3;
    }
  }
  shell_bi.biHeight = -sh;
  shell_bi.biSizeImage =
      static_cast<DWORD>(shell_stride) * static_cast<DWORD>(sh);
  return write_bmp_file(shell_bmp, shell_bi, shell_px.data(), shell_px.size());
}

}  // namespace detail
}  // namespace app
