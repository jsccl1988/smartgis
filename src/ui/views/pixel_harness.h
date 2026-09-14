// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PIXEL_HARNESS_H_
#define UI_VIEWS_PIXEL_HARNESS_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "ui/views/view.h"

namespace ui {
namespace views {

// BGRA8 pixels, row-major top-down (Windows DIB top-down convention).
struct PixelBuffer {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> bgra;
};

// Paints `root` into an offscreen bitmap at 96-DIP baseline (scale 1.0).
// Selects Segoe UI 12px on the DC so text matches Theme::measure_text_utf8.
PixelBuffer capture_view_tree(std::unique_ptr<View> root, int width, int height,
                              float device_scale_factor = 1.f);

struct PixelCompareOptions {
  // Per-channel tolerance for GDI antialiasing / minor driver drift.
  int max_channel_delta = 2;
  // Fail if more than this fraction of pixels exceed tolerance.
  double max_bad_pixel_fraction = 0.005;
};

struct PixelCompareResult {
  bool match = false;
  std::size_t bad_pixels = 0;
  std::string detail;
};

PixelCompareResult compare_pixel_buffers(const PixelBuffer& actual,
                                         const PixelBuffer& expected,
                                         PixelCompareOptions options = {});

// Repo-relative goldens: src/ui/views/testdata (cwd = repo root when run via build.bat te).
std::filesystem::path pixel_testdata_directory();

bool load_png_bgra(const std::filesystem::path& path, PixelBuffer* out,
                   std::string* error);
bool write_png_bgra(const std::filesystem::path& path,
                    const PixelBuffer& buffer, std::string* error);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PIXEL_HARNESS_H_
