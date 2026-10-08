// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Ordered tess runner for fill and line. One parallel_for, then merge by
// layer_ord so painter z does not depend on which worker finished first.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_TESS_JOBS_H_
#define VISTA_COMPONENT_MAP_LAYOUT_TESS_JOBS_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/component/map/draw.h"
#include "vista/component/map/layout.h"
#include "vista/component/map/layout/gen.h"
#include "vista/component/map/layout/tess_grain.h"

namespace vista {
namespace detail {

// |make_item| returns nullopt when the geom produces no mesh.
// Job::layer_ord selects the merge bucket. A stale layout_gen stops the
// serial walk and skips remaining workers; items already built still append.
template <typename Job, typename MakeItem>
void run_ordered_tess(const LayoutInput& in, size_t layer_count,
                      const std::vector<Job>& jobs, MakeItem&& make_item,
                      std::vector<DrawItem>* out) {
  if (!out || jobs.empty() || layer_count == 0) {
    return;
  }
  auto take = [&](const Job& job, DrawItem* slot) -> bool {
    std::optional<DrawItem> item = make_item(job);
    if (!item) {
      return false;
    }
    *slot = std::move(*item);
    return true;
  };
  if (!vista_layout_parallel_enabled() || jobs.size() < kParallelTessMinGeoms) {
    for (const Job& job : jobs) {
      if (layout_gen_stale(in)) {
        return;
      }
      DrawItem item;
      if (take(job, &item)) {
        out->push_back(std::move(item));
      }
    }
    return;
  }
  std::vector<DrawItem> items(jobs.size());
  std::vector<char> valid(jobs.size(), 0);
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, size_t{0}, jobs.size(),
      [&](size_t i) {
        if (layout_gen_stale(in)) {
          return;
        }
        if (take(jobs[i], &items[i])) {
          valid[i] = 1;
        }
      },
      kParallelTessGrain);
  std::vector<std::vector<DrawItem>> by_layer(layer_count);
  for (size_t i = 0; i < jobs.size(); ++i) {
    if (!valid[i]) {
      continue;
    }
    const size_t ord = jobs[i].layer_ord;
    if (ord >= layer_count) {
      continue;
    }
    by_layer[ord].push_back(std::move(items[i]));
  }
  for (std::vector<DrawItem>& bucket : by_layer) {
    for (DrawItem& item : bucket) {
      out->push_back(std::move(item));
    }
  }
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_TESS_JOBS_H_
