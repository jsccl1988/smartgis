// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/progress.h"

namespace plugin {
namespace detail {

void map2d_pixel_size(int* out_w, int* out_h) {
  // Fixed equal-profile size — no sticky MAP2D_SHOWCASE_W/H or CLI switches.
  if (out_w) {
    *out_w = kMap2dDefaultW;
  }
  if (out_h) {
    *out_h = kMap2dDefaultH;
  }
}

}  // namespace detail
}  // namespace plugin
