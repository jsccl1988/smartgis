// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Optional map horizon painted after the viewport overlay. Product map2d
// installs the pyramid, neatline, and scale bar here.

#ifndef UI_VIEWS_MAP_VIEWPORT_HORIZON_PAINT_H_
#define UI_VIEWS_MAP_VIEWPORT_HORIZON_PAINT_H_

#include "ui/ui_export.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ui {
namespace views {

using MapHorizonPaint = void (*)(HWND hwnd, HDC hdc, int width_px, int height_px);

UI_EXPORT void set_map_horizon_paint(MapHorizonPaint fn);
UI_EXPORT void paint_map_horizon(HWND hwnd, HDC hdc, int width_px, int height_px);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_HORIZON_PAINT_H_
