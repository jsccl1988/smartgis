// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/pixel_harness.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "render/skia/canvas.h"
#include "ui/views/theme.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {
namespace {

HFONT create_chrome_font() {
  return CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

bool read_bgra_from_bitmap(HBITMAP bitmap, int width, int height,
                           std::vector<std::uint8_t>* out) {
  if (!bitmap || width <= 0 || height <= 0 || !out) {
    return false;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  out->assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);

  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HGDIOBJ old = SelectObject(mem, bitmap);
  const int lines =
      GetDIBits(mem, bitmap, 0, static_cast<UINT>(height), out->data(), &bmi,
                DIB_RGB_COLORS);
  SelectObject(mem, old);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return lines == height;
}

}  // namespace

PixelBuffer capture_view_tree(std::unique_ptr<View> root, int width,
                              int height, float device_scale_factor) {
  PixelBuffer out;
  if (!root || width <= 0 || height <= 0) {
    return out;
  }

  Widget widget;
  widget.set_device_scale_factor(device_scale_factor);
  root->set_bounds({0, 0, width, height});
  widget.set_contents_view(std::move(root));
  View* painted = widget.contents_view();
  if (!painted) {
    return out;
  }
  painted->layout();

  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateCompatibleBitmap(screen, width, height);
  HGDIOBJ old_bmp = SelectObject(mem, bmp);
  HFONT font = create_chrome_font();
  HGDIOBJ old_font = SelectObject(mem, font);

  const render::skia::Color clear = Theme::current().chrome_bg;
  {
    render::skia::Canvas canvas(mem, width, height);
    canvas.fill_rect(0, 0, width, height, clear);
    painted->paint(&canvas);
  }

  SelectObject(mem, old_font);
  DeleteObject(font);
  out.width = width;
  out.height = height;
  if (!read_bgra_from_bitmap(bmp, width, height, &out.bgra)) {
    out.bgra.clear();
    out.width = 0;
    out.height = 0;
  }

  SelectObject(mem, old_bmp);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return out;
}

PixelCompareResult compare_pixel_buffers(const PixelBuffer& actual,
                                         const PixelBuffer& expected,
                                         PixelCompareOptions options) {
  PixelCompareResult result;
  if (actual.width != expected.width || actual.height != expected.height ||
      actual.bgra.size() != expected.bgra.size()) {
    result.detail = "dimension mismatch";
    return result;
  }
  if (actual.bgra.empty()) {
    result.detail = "empty actual buffer";
    return result;
  }

  const size_t pixel_count = actual.bgra.size() / 4u;
  size_t bad = 0;
  for (size_t i = 0; i < pixel_count; ++i) {
    const size_t o = i * 4u;
    const int db = std::abs(static_cast<int>(actual.bgra[o + 0]) -
                            static_cast<int>(expected.bgra[o + 0]));
    const int dg = std::abs(static_cast<int>(actual.bgra[o + 1]) -
                            static_cast<int>(expected.bgra[o + 1]));
    const int dr = std::abs(static_cast<int>(actual.bgra[o + 2]) -
                            static_cast<int>(expected.bgra[o + 2]));
    if (db > options.max_channel_delta || dg > options.max_channel_delta ||
        dr > options.max_channel_delta) {
      ++bad;
    }
  }
  result.bad_pixels = bad;
  const double fraction =
      pixel_count ? static_cast<double>(bad) / static_cast<double>(pixel_count)
                  : 0.0;
  if (fraction <= options.max_bad_pixel_fraction) {
    result.match = true;
  } else {
    result.detail = "too many mismatched pixels";
  }
  return result;
}

std::filesystem::path pixel_testdata_directory() {
  if (const wchar_t* env = _wgetenv(L"SMARTGIS_ROOT")) {
    std::filesystem::path root(env);
    if (std::filesystem::is_directory(root)) {
      return root / "src" / "ui" / "views" / "testdata";
    }
  }
  const std::filesystem::path relative = std::filesystem::path("src") / "ui" /
                                         "views" / "testdata";
  if (std::filesystem::is_directory(relative)) {
    return std::filesystem::absolute(relative);
  }
  return relative;
}

}  // namespace views
}  // namespace ui
