// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Linked when smt_has_skia is false so Canvas can always call Skia factories.

#include "ui/gfx/canvas/canvas_backend.h"

namespace ui {
namespace gfx {
namespace detail {

CanvasBackend* create_skia_canvas_backend(HDC, int, int) {
  return nullptr;
}

bool skia_canvas_backend_linked() {
  return false;
}

void discard_skia_retained_surface() {}

}  // namespace detail
}  // namespace gfx
}  // namespace ui
