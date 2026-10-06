// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/progress.h"

#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"

#include <cstdlib>
#include "base/process/switches.h"

namespace plugin {
namespace detail {
namespace {

thread_local HarnessShell* g_shell = nullptr;

}  // namespace

void bind_map2d_scenario_shell(HarnessShell* shell) {
  g_shell = shell;
}

HarnessShell* map2d_scenario_shell() {
  return g_shell;
}

void map2d_mark(const char* step) {
  if (g_shell) {
    g_shell->mark_named(kMarkMap2d, step, false);
  }
}

void map2d_pixel_size(int* out_w, int* out_h) {
  int w = kMap2dDefaultW;
  int h = kMap2dDefaultH;
  auto apply_dim = [](int* dest, int lo, int hi, const char* sw,
                      const char* env_key) {
    if (const char* ew = base::switch_cstr(sw); ew && ew[0] != '\0') {
      const int n = std::atoi(ew);
      if (n >= lo && n <= hi) {
        *dest = n;
        return;
      }
    }
    if (const char* ev = std::getenv(env_key); ev && ev[0] != '\0') {
      const int n = std::atoi(ev);
      if (n >= lo && n <= hi) {
        *dest = n;
      }
    }
  };
  apply_dim(&w, 320, 3840, "map2d-showcase-w", "MAP2D_SHOWCASE_W");
  apply_dim(&h, 240, 2160, "map2d-showcase-h", "MAP2D_SHOWCASE_H");
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

}  // namespace detail
}  // namespace plugin
