// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/common/progress.h"

#include "app/views/shell/harness/common/mark/mark.h"

#include <cstdlib>
#include "base/process/switches.h"

namespace app {
namespace detail {

void map2d_showcase_mark(const char* step) {
  write_mark(kMap2dShowcaseMarkLeaf, step, /*truncate=*/false);
}

void map2d_showcase_pixel_size(int* out_w, int* out_h) {
  int w = kMap2dShowcaseDefaultW;
  int h = kMap2dShowcaseDefaultH;
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
}  // namespace app
