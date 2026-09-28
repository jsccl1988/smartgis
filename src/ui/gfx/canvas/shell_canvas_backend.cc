// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/canvas/shell_canvas_backend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ui/gfx/canvas/canvas_backend.h"

namespace ui {
namespace gfx {
namespace {

ShellCanvasBackend g_requested = ShellCanvasBackend::kGdi;
ShellCanvasBackend g_resolved = ShellCanvasBackend::kGdi;

ShellCanvasBackend parse_token(const char* value) {
  if (!value || !*value) {
    return ShellCanvasBackend::kGdi;
  }
  if (std::strcmp(value, "skia") == 0) {
    return ShellCanvasBackend::kSkia;
  }
  if (std::strcmp(value, "gdi") == 0) {
    return ShellCanvasBackend::kGdi;
  }
  return ShellCanvasBackend::kGdi;
}

ShellCanvasBackend resolve(ShellCanvasBackend requested) {
  if (requested != ShellCanvasBackend::kSkia) {
    return ShellCanvasBackend::kGdi;
  }
  if (!detail::skia_canvas_backend_linked()) {
    std::fprintf(stderr,
                 "ui::gfx: --shell-canvas=skia requested but Skia is not "
                 "linked; falling back to gdi\n");
    return ShellCanvasBackend::kGdi;
  }
  return ShellCanvasBackend::kSkia;
}

}  // namespace

void set_shell_canvas_backend(ShellCanvasBackend backend) {
  g_requested = backend;
  g_resolved = resolve(backend);
}

ShellCanvasBackend shell_canvas_backend() {
  return g_requested;
}

ShellCanvasBackend resolved_shell_canvas_backend() {
  return g_resolved;
}

bool is_skia_backend_available() {
  return detail::skia_canvas_backend_linked();
}

ShellCanvasBackend apply_shell_canvas_preference(const char* cli_value) {
  ShellCanvasBackend requested = ShellCanvasBackend::kGdi;
  if (cli_value && *cli_value) {
    requested = parse_token(cli_value);
  } else if (const char* env = std::getenv("SMT_SHELL_CANVAS")) {
    requested = parse_token(env);
  }
  set_shell_canvas_backend(requested);
  return g_resolved;
}

const char* shell_canvas_backend_name(ShellCanvasBackend backend) {
  switch (backend) {
    case ShellCanvasBackend::kSkia:
      return "skia";
    case ShellCanvasBackend::kGdi:
    default:
      return "gdi";
  }
}

}  // namespace gfx
}  // namespace ui
