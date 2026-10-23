// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/viewport/horizon_paint.h"

#include <atomic>

namespace ui {
namespace views {
namespace {

std::atomic<MapHorizonPaint> g_paint{nullptr};

}  // namespace

void set_map_horizon_paint(MapHorizonPaint fn) {
  g_paint.store(fn, std::memory_order_release);
}

void paint_map_horizon(HWND hwnd, HDC hdc, int width_px, int height_px) {
  const MapHorizonPaint fn = g_paint.load(std::memory_order_acquire);
  if (!fn || width_px <= 0 || height_px <= 0) {
    return;
  }
  fn(hwnd, hdc, width_px, height_px);
}

}  // namespace views
}  // namespace ui
