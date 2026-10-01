// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/print/composer/print_composer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace plugin {
namespace {

void fill_rect_bits(uint8_t* bits, int w, int h, int stride, int x0, int y0,
                    int x1, int y1, COLORREF color) {
  x0 = std::max(0, x0);
  y0 = std::max(0, y0);
  x1 = std::min(w, x1);
  y1 = std::min(h, y1);
  const uint8_t b = GetBValue(color);
  const uint8_t g = GetGValue(color);
  const uint8_t r = GetRValue(color);
  for (int y = y0; y < y1; ++y) {
    const int row = h - 1 - y;
    uint8_t* p = bits + row * stride + x0 * 4;
    for (int x = x0; x < x1; ++x) {
      p[0] = b;
      p[1] = g;
      p[2] = r;
      p[3] = 0xff;
      p += 4;
    }
  }
}

void blit_map(uint8_t* dst, int dw, int dh, int dstride, int dx, int dy,
              int dw_box, int dh_box, const uint8_t* src, int sw, int sh,
              int sstride) {
  if (!src || sw <= 0 || sh <= 0 || sstride <= 0) {
    fill_rect_bits(dst, dw, dh, dstride, dx, dy, dx + dw_box, dy + dh_box,
                   RGB(0xf5, 0xf0, 0xe6));
    return;
  }
  for (int y = 0; y < dh_box; ++y) {
    const int sy = std::min(sh - 1, y * sh / std::max(1, dh_box));
    const int drow = dh - 1 - (dy + y);
    const int srow = sh - 1 - sy;
    uint8_t* dp = dst + drow * dstride + dx * 4;
    const uint8_t* sp = src + srow * sstride;
    for (int x = 0; x < dw_box; ++x) {
      const int sx = std::min(sw - 1, x * sw / std::max(1, dw_box));
      const uint8_t* s = sp + sx * 4;
      dp[0] = s[0];
      dp[1] = s[1];
      dp[2] = s[2];
      dp[3] = 0xff;
      dp += 4;
    }
  }
}

bool write_bmp(const std::string& path, int w, int h, const uint8_t* bits,
               int stride) {
  if (path.empty() || !bits || w <= 0 || h <= 0 || stride <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride) * static_cast<DWORD>(h);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = w;
  bih.biHeight = h;
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
    return false;
  }
  const bool ok =
      std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
      std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
      std::fwrite(bits, 1, image_bytes, f) == image_bytes;
  std::fclose(f);
  return ok;
}

std::wstring to_wide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring w(n > 0 ? static_cast<size_t>(n - 1) : 0, L'\0');
  if (n > 1) {
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
  }
  return w;
}

std::string auto_scale_label(double units_per_px, int bar_px) {
  const double world = std::abs(units_per_px) * static_cast<double>(bar_px);
  char buf[64];
  if (world >= 1000.0) {
    std::snprintf(buf, sizeof(buf), "%.0f km", world / 1000.0);
  } else if (world >= 1.0) {
    std::snprintf(buf, sizeof(buf), "%.0f m", world);
  } else {
    std::snprintf(buf, sizeof(buf), "1:%.0f",
                  1.0 / std::max(1e-9, units_per_px));
  }
  return buf;
}

}  // namespace

bool PrintComposer::compose(const PrintComposerInput& in,
                            std::vector<uint8_t>* out_bgra, int* out_w,
                            int* out_h, int* out_stride) {
  if (!out_bgra || !out_w || !out_h || !out_stride) {
    return false;
  }
  const int w = in.page_width_px;
  const int h = in.page_height_px;
  if (w < 200 || h < 200) {
    return false;
  }
  const int margin = 24;
  const int legend_w = 160;
  const int footer_h = 48;
  const int map_x = margin;
  const int map_y = margin;
  const int map_w = w - margin * 2 - legend_w - 12;
  const int map_h = h - margin * 2 - footer_h;
  if (map_w < 64 || map_h < 64) {
    return false;
  }

  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  if (!mem) {
    ReleaseDC(nullptr, screen);
    return false;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits_void = nullptr;
  HBITMAP dib =
      CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits_void, nullptr, 0);
  if (!dib || !bits_void) {
    if (dib) {
      DeleteObject(dib);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return false;
  }
  auto* bits = static_cast<uint8_t*>(bits_void);
  const int stride = ((w * 32 + 31) / 32) * 4;
  HGDIOBJ old = SelectObject(mem, dib);

  fill_rect_bits(bits, w, h, stride, 0, 0, w, h, RGB(255, 255, 255));
  blit_map(bits, w, h, stride, map_x, map_y, map_w, map_h, in.map_bgra,
           in.map_width_px, in.map_height_px, in.map_stride_bytes);
  fill_rect_bits(bits, w, h, stride, map_x, map_y, map_x + map_w, map_y + 2,
                 RGB(0x40, 0x40, 0x40));
  fill_rect_bits(bits, w, h, stride, map_x, map_y + map_h - 2, map_x + map_w,
                 map_y + map_h, RGB(0x40, 0x40, 0x40));
  fill_rect_bits(bits, w, h, stride, map_x, map_y, map_x + 2, map_y + map_h,
                 RGB(0x40, 0x40, 0x40));
  fill_rect_bits(bits, w, h, stride, map_x + map_w - 2, map_y, map_x + map_w,
                 map_y + map_h, RGB(0x40, 0x40, 0x40));

  const int lx = map_x + map_w + 12;
  const int ly = map_y;
  fill_rect_bits(bits, w, h, stride, lx, ly, lx + legend_w, ly + map_h,
                 RGB(0xf4, 0xf4, 0xf4));

  SetBkMode(mem, TRANSPARENT);
  SetTextColor(mem, RGB(0x20, 0x20, 0x20));
  HFONT font =
      CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old_font = font ? SelectObject(mem, font) : nullptr;
  TextOutW(mem, lx + 8, ly + 4, L"Legend", 6);

  int row_y = ly + 28;
  for (size_t i = 0; i < in.legend.size() && row_y + 16 < ly + map_h; ++i) {
    const uint32_t rgba = in.legend[i].rgba;
    const COLORREF swatch =
        RGB(static_cast<BYTE>((rgba >> 16) & 0xff),
            static_cast<BYTE>((rgba >> 8) & 0xff),
            static_cast<BYTE>(rgba & 0xff));
    fill_rect_bits(bits, w, h, stride, lx + 8, row_y, lx + 28, row_y + 14,
                   swatch);
    const std::wstring label = to_wide(in.legend[i].label);
    if (!label.empty()) {
      TextOutW(mem, lx + 36, row_y, label.c_str(),
               static_cast<int>(label.size()));
    }
    row_y += 22;
  }

  const int bar_x = map_x;
  const int bar_y = map_y + map_h + 12;
  const int bar_len = 120;
  fill_rect_bits(bits, w, h, stride, bar_x, bar_y, bar_x + bar_len, bar_y + 8,
                 RGB(0x20, 0x20, 0x20));
  fill_rect_bits(bits, w, h, stride, bar_x, bar_y - 4, bar_x + 2, bar_y + 12,
                 RGB(0x20, 0x20, 0x20));
  fill_rect_bits(bits, w, h, stride, bar_x + bar_len - 2, bar_y - 4,
                 bar_x + bar_len, bar_y + 12, RGB(0x20, 0x20, 0x20));
  const std::string label =
      in.scale_label.empty()
          ? auto_scale_label(in.map_units_per_px, bar_len)
          : in.scale_label;
  const std::wstring wlabel = to_wide(label);
  TextOutW(mem, bar_x + bar_len + 8, bar_y - 2, wlabel.c_str(),
           static_cast<int>(wlabel.size()));
  TextOutW(mem, bar_x, bar_y + 14, L"Scale", 5);

  if (old_font) {
    SelectObject(mem, old_font);
  }
  if (font) {
    DeleteObject(font);
  }

  out_bgra->assign(bits, bits + static_cast<size_t>(stride) * static_cast<size_t>(h));
  *out_w = w;
  *out_h = h;
  *out_stride = stride;

  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return true;
}

bool PrintComposer::export_page_bmp(const PrintComposerInput& in,
                                    const std::string& path) {
  std::vector<uint8_t> bgra;
  int w = 0;
  int h = 0;
  int stride = 0;
  if (!compose(in, &bgra, &w, &h, &stride)) {
    return false;
  }
  return write_bmp(path, w, h, bgra.data(), stride);
}

}  // namespace plugin
