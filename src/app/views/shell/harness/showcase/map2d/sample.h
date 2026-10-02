// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SAMPLE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SAMPLE_H_

#include <cstddef>

namespace app {

class Browser;

namespace detail {

inline constexpr int kMap2dShowcaseDefaultW = 640;
inline constexpr int kMap2dShowcaseDefaultH = 480;

void map2d_showcase_mark(const char* step);

// Optional SMT_MAP2D_SHOWCASE_W / SMT_MAP2D_SHOWCASE_H (matrix uses 1280x720).
void map2d_showcase_pixel_size(int* out_w, int* out_h);

bool try_open_china_sample(Browser& browser);

bool try_path_candidates(const wchar_t* const* rels,
                         size_t count,
                         char* out_utf8,
                         size_t out_cap);

bool try_load_align_style(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SAMPLE_H_
