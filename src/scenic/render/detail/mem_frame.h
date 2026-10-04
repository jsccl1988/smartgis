// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_SCENIC_DETAIL_MEM_FRAME_H_
#define SMT_SCENIC_DETAIL_MEM_FRAME_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace scenic {
namespace detail {

// In-memory 32bpp DIB for Scenic software present. BitBlt reuse when the
// fingerprint (view/orbit + draw set + size) is unchanged.
struct MemFrame {
  HDC dc = nullptr;
  HBITMAP bmp = nullptr;
  HGDIOBJ old = nullptr;
  void* bits = nullptr;
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  uint64_t fingerprint = 0;

  ~MemFrame() { destroy(); }

  MemFrame() = default;
  MemFrame(const MemFrame&) = delete;
  MemFrame& operator=(const MemFrame&) = delete;

  void destroy() {
    if (dc && old) {
      SelectObject(dc, old);
      old = nullptr;
    }
    if (bmp) {
      DeleteObject(bmp);
      bmp = nullptr;
    }
    if (dc) {
      DeleteDC(dc);
      dc = nullptr;
    }
    bits = nullptr;
    width_px = 0;
    height_px = 0;
    fingerprint = 0;
  }

  bool ensure(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) {
      return false;
    }
    if (dc && bmp && bits && width_px == w && height_px == h) {
      return true;
    }
    destroy();
    HDC screen = GetDC(nullptr);
    if (!screen) {
      return false;
    }
    dc = CreateCompatibleDC(screen);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h);  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!dc || !bmp || !bits) {
      destroy();
      return false;
    }
    old = SelectObject(dc, bmp);
    width_px = w;
    height_px = h;
    return true;
  }

  bool blit_to(HDC dst) const {
    if (!dst || !dc || !bmp || width_px == 0 || height_px == 0) {
      return false;
    }
    return BitBlt(dst, 0, 0, static_cast<int>(width_px),
                  static_cast<int>(height_px), dc, 0, 0, SRCCOPY) != FALSE;
  }

  bool matches(uint32_t w, uint32_t h, uint64_t fp) const {
    return dc && bmp && bits && width_px == w && height_px == h &&
           fingerprint == fp && fp != 0;
  }
};

inline uint64_t mix_u64(uint64_t h, uint64_t v) {
  h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
  return h;
}

}  // namespace detail
}  // namespace scenic

#endif  // SMT_SCENIC_DETAIL_MEM_FRAME_H_
