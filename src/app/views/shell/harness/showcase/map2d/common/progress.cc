// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/common/progress.h"

#include "app/views/shell/harness/common/mark/mark.h"

#include <cstdlib>

namespace app {
namespace detail {

void map2d_showcase_mark(const char* step) {
  write_mark(kMap2dShowcaseMarkLeaf, step, /*truncate=*/false);
}

void map2d_showcase_pixel_size(int* out_w, int* out_h) {
  int w = kMap2dShowcaseDefaultW;
  int h = kMap2dShowcaseDefaultH;
  if (const char* ew = std::getenv("SMT_MAP2D_SHOWCASE_W");
      ew && ew[0] != '\0') {
    const int n = std::atoi(ew);
    if (n >= 320 && n <= 3840) {
      w = n;
    }
  }
  if (const char* eh = std::getenv("SMT_MAP2D_SHOWCASE_H");
      eh && eh[0] != '\0') {
    const int n = std::atoi(eh);
    if (n >= 240 && n <= 2160) {
      h = n;
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

}  // namespace detail
}  // namespace app
