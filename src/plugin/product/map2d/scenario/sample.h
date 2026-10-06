// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_SAMPLE_H_
#define PLUGIN_MAP2D_SCENARIO_SAMPLE_H_

#include <cstddef>

namespace plugin {

class HarnessShell;

namespace detail {

bool try_open_china_sample(HarnessShell& browser);
bool try_path_candidates(HarnessShell& browser, const wchar_t* const* rels,
                         size_t count, char* out_utf8, size_t out_cap);
bool try_load_align_style(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_SAMPLE_H_
