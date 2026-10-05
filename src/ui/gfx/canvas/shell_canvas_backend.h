// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_CANVAS_SHELL_CANVAS_BACKEND_H_
#define UI_GFX_CANVAS_SHELL_CANVAS_BACKEND_H_

// Process-wide shell Canvas backend preference (GDI vs optional Skia).
// Applied once at process start from CLI / env. See
// docs/superpowers/specs/2026-09-14-render-skia-canvas-design.md § runtime.

#include "ui/ui_export.h"
namespace ui {
namespace gfx {

enum class ShellCanvasBackend {
  kGdi,
  kSkia,
};

// Requested preference (may differ from resolved when Skia is unavailable).
UI_EXPORT void set_shell_canvas_backend(ShellCanvasBackend backend);
UI_EXPORT ShellCanvasBackend shell_canvas_backend();

// Backend actually used for new Canvas instances after fallback.
UI_EXPORT ShellCanvasBackend resolved_shell_canvas_backend();

// True when this binary linked a real Skia paint TU (has_skia + pin).
UI_EXPORT bool is_skia_backend_available();

// Resolve preference: non-empty |cli_value| wins; else env SHELL_CANVAS;
// else gdi. Invalid tokens → gdi. Requested skia without capability → gdi and
// one stderr line. Returns the resolved backend.
UI_EXPORT ShellCanvasBackend apply_shell_canvas_preference(const char* cli_value);

UI_EXPORT const char* shell_canvas_backend_name(ShellCanvasBackend backend);

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_CANVAS_SHELL_CANVAS_BACKEND_H_
