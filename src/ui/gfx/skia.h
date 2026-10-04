// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_SKIA_H_
#define UI_GFX_SKIA_H_

// Shell paint umbrella. Runtime backend: shell_canvas_backend.h (GDI default;
// optional Skia when smt_has_skia + pin). See docs/superpowers/ui-views-skia.md.

#include "ui/ui_export.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace gfx {

UI_EXPORT const char* module_id();

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_SKIA_H_
