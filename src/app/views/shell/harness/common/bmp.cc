// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/bmp.h"

#include "app/views/shell/harness/common/pump.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#ifndef PW_CLIENTONLY
#define PW_CLIENTONLY 0x00000001
#endif
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace app {
namespace detail {
namespace {

bool read_bmp_pixels(FILE* in,
                     BITMAPFILEHEADER* fh,
                     BITMAPINFOHEADER* bi,
                     std::vector<unsigned char>* pixels,
                     int* out_w,
                     int* out_h,
                     const BmpFileCheckOpts& opts) {
  if (std::fread(fh, sizeof(*fh), 1, in) != 1 ||
      std::fread(bi, sizeof(*bi), 1, in) != 1 || fh->bfType != 0x4D42) {
    return false;
  }
  const int w = bi->biWidth;
  const int h = bi->biHeight < 0 ? -bi->biHeight : bi->biHeight;
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (w < opts.min_w || h < opts.min_h) {
    return false;
  }
  if (bi->biBitCount == 24) {
    // ok
  } else if (opts.allow_32bpp && bi->biBitCount == 32) {
    // ok
  } else {
    return false;
  }
  const int stride = ((w * bi->biBitCount + 31) / 32) * 4;
  pixels->assign(static_cast<size_t>(stride) * static_cast<size_t>(h), 0);
  if (std::fseek(in, static_cast<long>(fh->bfOffBits), SEEK_SET) != 0 ||
      std::fread(pixels->data(), 1, pixels->size(), in) != pixels->size()) {
    return false;
  }
  return true;
}

bool file_pixels_pass(const unsigned char* pixels,
                      int stride,
                      int w,
                      int h,
                      int bpp,
                      const BmpFileCheckOpts& opts) {
  int lit = 0;
  int samples = 0;
  const int step_x = (std::max)(1, w / 32);
  const int step_y = (std::max)(1, h / 24);
  for (int y = 0; y < h; y += step_y) {
    const unsigned char* row = pixels + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += step_x) {
      const unsigned char b = row[x * bpp + 0];
      const unsigned char g = row[x * bpp + 1];
      const unsigned char r = row[x * bpp + 2];
      ++samples;
      if (static_cast<int>(r) + g + b > 24) {
        ++lit;
      }
    }
  }
  if (!(samples > 0 && lit * 20 >= samples)) {
    return false;
  }
  if (!opts.require_color_diversity) {
    return true;
  }
  // Reject flat clear-color frames (geometry never reached the swapchain).
  int unique = 0;
  unsigned char seen[64][3] = {};
  const int ux = (std::max)(1, w / 24);
  const int uy = (std::max)(1, h / 18);
  for (int y = 0; y < h; y += uy) {
    const unsigned char* row = pixels + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += ux) {
      const unsigned char b = row[x * bpp + 0];
      const unsigned char g = row[x * bpp + 1];
      const unsigned char r = row[x * bpp + 2];
      bool found = false;
      for (int i = 0; i < unique; ++i) {
        if (seen[i][0] == r && seen[i][1] == g && seen[i][2] == b) {
          found = true;
          break;
        }
      }
      if (!found && unique < 64) {
        seen[unique][0] = r;
        seen[unique][1] = g;
        seen[unique][2] = b;
        ++unique;
      }
    }
  }
  return unique >= 2;
}

}  // namespace

bool pixels_have_visible_signal(const unsigned char* pixels,
                                        int stride,
                                        int w,
                                        int h,
                                        VisiblePolicy policy) {
  if (!pixels || stride <= 0 || w < 8 || h < 8) {
    return false;
  }
  if (policy == VisiblePolicy::kSparseDistinct) {
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
        const unsigned packed = (static_cast<unsigned>(r) << 16) |
                                (static_cast<unsigned>(g) << 8) |
                                static_cast<unsigned>(b);
        if (packed != last) {
          ++distinct;
          last = packed;
        }
      }
    }
    return non_near_black > 32 && distinct > 4;
  }

  // kGridLitFraction â€?atmosphere grid (>=5% lit); require larger frame.
  if (w < 32 || h < 32 || stride < w * 3) {
    return false;
  }
  int lit = 0;
  int samples = 0;
  const int step_x = (std::max)(1, w / 32);
  const int step_y = (std::max)(1, h / 24);
  for (int y = 0; y < h; y += step_y) {
    const unsigned char* row = pixels + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += step_x) {
      const unsigned char b = row[x * 3 + 0];
      const unsigned char g = row[x * 3 + 1];
      const unsigned char r = row[x * 3 + 2];
      ++samples;
      if (static_cast<int>(r) + g + b > 24) {
        ++lit;
      }
    }
  }
  return samples > 0 && lit * 20 >= samples;
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
      // Active Map tab accent (#007acc) required for a finished shell paint.
      if (r < 40 && g > 90 && g < 160 && b > 170 && b > r + 100) {
        ++accent;
      }
    }
  }
  return used >= 4 && accent >= 6;
}

bool blit_client_to_dib(HWND hwnd, HDC mem, int w, int h) {
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  POINT origin = {0, 0};
  ClientToScreen(hwnd, &origin);
  const BOOL ok = BitBlt(mem, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY);
  ReleaseDC(nullptr, screen);
  return ok != FALSE;
}

bool capture_hwnd_bmp(HWND hwnd,
                              const wchar_t* filename,
                              const CaptureOpts& opts) {
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
  bool accept = false;
  const int attempts = opts.max_attempts < 1 ? 1 : opts.max_attempts;
  for (int attempt = 0; attempt < attempts && !accept; ++attempt) {
    if (opts.pump_base_ms > 0 || opts.pump_step_ms > 0) {
      RedrawWindow(hwnd, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
      pump_messages(opts.pump_base_ms +
                            static_cast<DWORD>(attempt) * opts.pump_step_ms);
    }

    BOOL printed =
        PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
    }
    (void)printed;
    got = GetDIBits(mem, bmp, 0, h, pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    auto pixels_ok = [&]() {
      if (got != h) {
        return false;
      }
      if (!pixels_have_visible_signal(pixels.data(), stride, w, h,
                                              opts.visible)) {
        return false;
      }
      if (opts.require_chrome_diversity &&
          !bmp_has_chrome_diversity(pixels.data(), stride, w, h)) {
        return false;
      }
      return true;
    };
    if (!pixels_ok()) {
      if (blit_client_to_dib(hwnd, mem, w, h)) {
        got = GetDIBits(mem, bmp, 0, h, pixels.data(),
                        reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
      }
    }
    accept = pixels_ok();
  }

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, wnd_dc);
  // Match prior showcase behavior: write when GetDIBits filled the buffer;
  // visible/chrome checks only drive retry + blit fallback.
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

bool bmp_file_has_visible_signal(const wchar_t* filename,
                                         int* out_w,
                                         int* out_h,
                                         const BmpFileCheckOpts& opts) {
  if (!filename) {
    return false;
  }
  FILE* in = nullptr;
  if (_wfopen_s(&in, filename, L"rb") != 0 || !in) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER bi{};
  std::vector<unsigned char> pixels;
  if (!read_bmp_pixels(in, &fh, &bi, &pixels, out_w, out_h, opts)) {
    std::fclose(in);
    return false;
  }
  std::fclose(in);
  const int w = bi.biWidth;
  const int h = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
  const int bpp = bi.biBitCount / 8;
  const int stride = ((w * bi.biBitCount + 31) / 32) * 4;
  return file_pixels_pass(pixels.data(), stride, w, h, bpp, opts);
}

bool bmp_file_has_visible_signal_a(const char* filename,
                                           int* out_w,
                                           int* out_h,
                                           const BmpFileCheckOpts& opts) {
  if (!filename) {
    return false;
  }
  FILE* in = nullptr;
  if (fopen_s(&in, filename, "rb") != 0 || !in) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER bi{};
  std::vector<unsigned char> pixels;
  if (!read_bmp_pixels(in, &fh, &bi, &pixels, out_w, out_h, opts)) {
    std::fclose(in);
    return false;
  }
  std::fclose(in);
  const int w = bi.biWidth;
  const int h = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
  const int bpp = bi.biBitCount / 8;
  const int stride = ((w * bi.biBitCount + 31) / 32) * 4;
  return file_pixels_pass(pixels.data(), stride, w, h, bpp, opts);
}

}  // namespace detail
}  // namespace app
