// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_MAP2D_COMMON_PROGRESS_H_
#define APP_VIEWS_HARNESS_SHOWCASE_MAP2D_COMMON_PROGRESS_H_

namespace app {
namespace detail {

inline constexpr int kMap2dShowcaseDefaultW = 1280;
inline constexpr int kMap2dShowcaseDefaultH = 720;

// Appends a progress step to map2d-showcase-mark.txt (captures/).
void map2d_showcase_mark(const char* step);

// Optional MAP2D_SHOWCASE_W / MAP2D_SHOWCASE_H (matrix uses 1280x720).
void map2d_showcase_pixel_size(int* out_w, int* out_h);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_MAP2D_COMMON_PROGRESS_H_
