// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SEED_SAMPLE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SEED_SAMPLE_H_

#include <cstddef>

namespace app {

class Browser;

namespace detail {

bool try_open_china_sample(Browser& browser);

bool try_path_candidates(const wchar_t* const* rels,
                         size_t count,
                         char* out_utf8,
                         size_t out_cap);

bool try_load_align_style(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SEED_SAMPLE_H_
