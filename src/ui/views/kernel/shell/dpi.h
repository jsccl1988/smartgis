// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_SHELL_DPI_H_
#define UI_VIEWS_KERNEL_SHELL_DPI_H_

#include "ui/ui_views_export.h"
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

// Logical (96 DPI) baseline used by Views preferred sizes authored in DIPs.
constexpr unsigned kDefaultDpi = 96;

// dpi / 96. Unknown or zero DPI maps to 1.0.
inline float scale_factor_from_dpi(unsigned dpi) {
  if (dpi == 0) {
    return 1.f;
  }
  return static_cast<float>(dpi) / static_cast<float>(kDefaultDpi);
}

// Round DIP → physical pixels at the given scale factor.
// Half-away-from-zero (std::lround) so 10 DIP at 1.25 is 13.
inline int dip_to_px(int dip, float scale_factor) {
  constexpr float kEpsilon = 0.0001f;
  if (scale_factor <= kEpsilon) {
    return dip;
  }
  return static_cast<int>(std::lround(static_cast<double>(dip) * scale_factor));
}

// Round physical pixels → DIP at the given scale factor.
inline int px_to_dip(int px, float scale_factor) {
  constexpr float kEpsilon = 0.0001f;
  if (scale_factor <= kEpsilon) {
    return px;
  }
  return static_cast<int>(std::lround(static_cast<double>(px) / scale_factor));
}

// Prefer GetDpiForWindow; fall back to monitor / system LOGPIXELSX.
UI_VIEWS_EXPORT unsigned dpi_for_hwnd(HWND hwnd);

// Clamp a top-level window box so it stays inside the monitor work area near
// |anchor| (owner HWND or nullptr for the primary work area).
UI_VIEWS_EXPORT void clamp_rect_to_work_area(int* x, int* y, int width, int height, HWND anchor);

// Best-effort Per-Monitor V2, then per-monitor, then system DPI aware.
// Safe to call more than once; returns true if any awareness mode engaged.
UI_VIEWS_EXPORT bool enable_process_dpi_awareness();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_SHELL_DPI_H_
