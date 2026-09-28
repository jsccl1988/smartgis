// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Sync parallel_for over a Mogu Executor (execute(), not folly add()).
// Capability layer: depends on executor + detail + std only.

#ifndef BASE_EXECUTION_PARALLEL_FOR_H
#define BASE_EXECUTION_PARALLEL_FOR_H

#include <cstddef>
#include <exception>
#include <memory>
#include <utility>

#include "base/execution/parallel/detail/completion_latch.h"
#include "base/execution/parallel/detail/grain.h"

namespace base {
namespace execution {

template <typename Executor, typename Index, typename Body>
void parallel_for(Executor& executor, Index first, Index last, Body body,
                  std::size_t grain = 0, std::size_t worker_hint = 0) {
  if (!(first < last)) {
    return;
  }
  const Index n = static_cast<Index>(last - first);
  grain = detail::resolve_grain(n, grain, worker_hint);

  auto body_ptr = std::make_shared<Body>(std::move(body));
  detail::completion_latch latch;
  detail::for_each_chunk(first, last, grain, [&](Index a, Index b) {
    latch.add(1);
    // Discard execute()'s future; join is the latch (do not include futures/).
    (void)executor.execute([&, a, b, body_ptr]() {
      try {
        for (Index i = a; i < b; ++i) {
          (*body_ptr)(i);
        }
      } catch (...) {
        latch.set_exception(std::current_exception());
      }
      latch.count_down();
    });
  });
  latch.wait();
}

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_PARALLEL_FOR_H
