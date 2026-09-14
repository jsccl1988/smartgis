// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DPI_H_
#define UI_VIEWS_DPI_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

// Logical (96 DPI) baseline used by Views preferred sizes authored in DIPs.
constexpr unsigned kDefaultDpi = 96;

// dpi / 96. Unknown or zero DPI maps to 1.0.
float scale_factor_from_dpi(unsigned dpi);

// Round DIP → physical pixels at the given scale factor.
int dip_to_px(int dip, float scale_factor);

// Round physical pixels → DIP at the given scale factor.
int px_to_dip(int px, float scale_factor);

// Prefer GetDpiForWindow; fall back to monitor / system LOGPIXELSX.
unsigned dpi_for_hwnd(HWND hwnd);

// Clamp a top-level window box so it stays inside the monitor work area near
// |anchor| (owner HWND or nullptr for the primary work area).
void clamp_rect_to_work_area(int* x, int* y, int width, int height, HWND anchor);

// Best-effort Per-Monitor V2, then per-monitor, then system DPI aware.
// Safe to call more than once; returns true if any awareness mode engaged.
bool enable_process_dpi_awareness();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DPI_H_
