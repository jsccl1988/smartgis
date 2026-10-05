// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/capture/shell_capture.h"

#include "app/views/shell/harness/self_test/self_test.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace app {
namespace detail {
namespace {

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

// PrintWindow of the shell often leaves the child DrawHost HWND as a dark
// hole. Blit the map client into the shell DIB at its client-relative origin.
bool composite_map_hwnd_into_shell(HWND shell,
                                   HWND map,
                                   HDC mem,
                                   int shell_w,
                                   int shell_h) {
  if (!shell || !map || !mem || !IsWindow(shell) || !IsWindow(map) ||
      shell_w < 8 || shell_h < 8) {
    return false;
  }
  RECT map_client = {};
  if (!GetClientRect(map, &map_client)) {
    return false;
  }
  const int mw = map_client.right - map_client.left;
  const int mh = map_client.bottom - map_client.top;
  if (mw < 8 || mh < 8) {
    return false;
  }
  POINT origin = {0, 0};
  if (!ClientToScreen(map, &origin)) {
    return false;
  }
  POINT shell_origin = {0, 0};
  if (!ClientToScreen(shell, &shell_origin)) {
    return false;
  }
  const int dst_x = origin.x - shell_origin.x;
  const int dst_y = origin.y - shell_origin.y;
  if (dst_x >= shell_w || dst_y >= shell_h) {
    return false;
  }
  const int copy_w = std::min(mw, shell_w - std::max(0, dst_x));
  const int copy_h = std::min(mh, shell_h - std::max(0, dst_y));
  if (copy_w < 8 || copy_h < 8) {
    return false;
  }
  HDC map_dc = GetDC(map);
  if (!map_dc) {
    return false;
  }
  const int src_x = dst_x < 0 ? -dst_x : 0;
  const int src_y = dst_y < 0 ? -dst_y : 0;
  const int blit_x = std::max(0, dst_x);
  const int blit_y = std::max(0, dst_y);
  const BOOL gdi_ok =
      BitBlt(mem, blit_x, blit_y, copy_w, copy_h, map_dc, src_x, src_y,
             SRCCOPY);
  ReleaseDC(map, map_dc);
  BOOL ok = gdi_ok;
  const bool dxgi_flip =
      (GetWindowLongPtrW(map, GWL_EXSTYLE) & WS_EX_NOREDIRECTIONBITMAP) != 0;
  // Flip-model DXGI has no GDI redirection bitmap. Copy DWM screen pixels of
  // the present client while the shell is TOPMOST (caller). Do not blit the
  // whole desktop — only this client rect.
  if (dxgi_flip) {
    HDC screen = GetDC(nullptr);
    if (screen) {
      ok = BitBlt(mem, blit_x, blit_y, copy_w, copy_h, screen,
                  origin.x + src_x, origin.y + src_y, SRCCOPY | CAPTUREBLT);
      ReleaseDC(nullptr, screen);
    }
  }
  return ok != FALSE;
}

bool composite_hud_hwnd_into_shell(HWND shell,
                                   HWND hud,
                                   HDC mem,
                                   int shell_w,
                                   int shell_h) {
  if (!shell || !hud || !mem || !IsWindow(hud)) {
    return false;
  }
  RECT hud_rc = {};
  if (!GetClientRect(hud, &hud_rc)) {
    return false;
  }
  const int hw = hud_rc.right - hud_rc.left;
  const int hh = hud_rc.bottom - hud_rc.top;
  if (hw < 8 || hh < 4) {
    return false;
  }
  POINT origin = {0, 0};
  POINT shell_origin = {0, 0};
  if (!ClientToScreen(hud, &origin) || !ClientToScreen(shell, &shell_origin)) {
    return false;
  }
  const int dst_x = origin.x - shell_origin.x;
  const int dst_y = origin.y - shell_origin.y;
  if (dst_x >= shell_w || dst_y >= shell_h) {
    return false;
  }
  const int copy_w = std::min(hw, shell_w - std::max(0, dst_x));
  const int copy_h = std::min(hh, shell_h - std::max(0, dst_y));
  if (copy_w < 8 || copy_h < 4) {
    return false;
  }
  HDC tmp = CreateCompatibleDC(mem);
  HBITMAP tmp_bmp = tmp ? CreateCompatibleBitmap(mem, hw, hh) : nullptr;
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
        printed = BitBlt(tmp, 0, 0, hw, hh, hdc, 0, 0, SRCCOPY);
        ReleaseDC(hud, hdc);
      }
    }
    if (printed) {
      BitBlt(mem, std::max(0, dst_x), std::max(0, dst_y), copy_w, copy_h, tmp,
             dst_x < 0 ? -dst_x : 0, dst_y < 0 ? -dst_y : 0, SRCCOPY);
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

bool bmp_has_shell_diversity(const unsigned char* pixels,
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

}  // namespace

const wchar_t* ui_showcase_bmp_leaf(UiShowcaseMode mode) {
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
  auto pin_topmost = [](HWND h, bool on) {
    if (h && IsWindow(h)) {
      SetWindowPos(h, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }
  };
  const bool interact_leaf =
      filename && wcsstr(filename, L"ui-showcase-interact") != nullptr;
  pin_topmost(hwnd, true);
  // Do not TOPMOST the map child for interact: it covers Skia chrome and
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
              bmp_has_shell_diversity(pixels.data(), stride, w, h);
    if (diverse) {
      break;
    }
    // Window-DC blit only when PrintWindow produced no visible pixels.
    // Never fall back to the desktop DC (overlapping windows polluted BMPs).
    if (!signal && blit_client_to_dib(hwnd, wnd_dc, mem, w, h)) {
      (void)composite_map_hwnd_into_shell(hwnd, map_hwnd, mem, w, h);
      (void)composite_hud_hwnd_into_shell(hwnd, hud_hwnd, mem, w, h);
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
                bmp_has_shell_diversity(pixels.data(), stride, w, h);
    }
  }
  pin_topmost(hud_hwnd, false);
  pin_topmost(map_hwnd, false);
  pin_topmost(hwnd, false);
  if (!diverse && have_signal) {
    pixels = std::move(best_signal);
    got = h;
  }

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, wnd_dc);
  // Reject flat fills (e.g. scene PrintWindow under DXGI present) — writing
  // them as bmp-ok hid a blank ui.scene capture from the harness.
  const bool diverse_ok = bmp_has_shell_diversity(pixels.data(), stride, w, h);
  if (got != h ||
      !pixels_have_visible_signal(pixels.data(), stride, w, h) ||
      !diverse_ok) {
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

}  // namespace detail
}  // namespace app
