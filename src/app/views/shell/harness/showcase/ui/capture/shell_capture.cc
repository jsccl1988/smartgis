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

// PrintWindow of the shell often leaves the child MapViewport HWND as a dark
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
  const BOOL ok =
      BitBlt(mem, blit_x, blit_y, copy_w, copy_h, map_dc, src_x, src_y,
             SRCCOPY);
  ReleaseDC(map, map_dc);
  return ok != FALSE;
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

bool capture_ui_shell_bmp(HWND hwnd, const wchar_t* filename, HWND map_hwnd) {
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
    if (printed) {
      (void)composite_map_hwnd_into_shell(hwnd, map_hwnd, mem, w, h);
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
  if (got != h ||
      !pixels_have_visible_signal(pixels.data(), stride, w, h) ||
      !bmp_has_shell_diversity(pixels.data(), stride, w, h)) {
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
