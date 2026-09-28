// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Fixed-grain partition helpers for the data-parallel capability layer.
// Std-only: do not include executor, futures, async, pipeline, or map_reduce.

#ifndef BASE_EXECUTION_PARALLEL_DETAIL_GRAIN_H
#define BASE_EXECUTION_PARALLEL_DETAIL_GRAIN_H

#include <algorithm>
#include <cstddef>
#include <thread>

namespace base {
namespace execution {
namespace detail {

inline std::size_t worker_hint_or_hardware(std::size_t worker_hint) {
  if (worker_hint > 0) {
    return worker_hint;
  }
  const unsigned hc = std::thread::hardware_concurrency();
  return hc == 0 ? 1 : static_cast<std::size_t>(hc);
}

template <typename Index>
std::size_t default_grain(Index n, std::size_t worker_hint) {
  if (!(n > Index{0})) {
    return 1;
  }
  const std::size_t workers = worker_hint_or_hardware(worker_hint);
  const std::size_t span = static_cast<std::size_t>(n);
  return std::max<std::size_t>(1, span / (workers * 4));
}

template <typename Index>
std::size_t resolve_grain(Index n, std::size_t grain, std::size_t worker_hint) {
  if (grain > 0) {
    return grain;
  }
  return default_grain(n, worker_hint);
}

// Invokes fn(chunk_first, chunk_last) for each fixed-size chunk of [first, last).
template <typename Index, typename Fn>
void for_each_chunk(Index first, Index last, std::size_t grain, Fn&& fn) {
  if (!(first < last)) {
    return;
  }
  if (grain == 0) {
    grain = 1;
  }
  for (Index i = first; i < last;) {
    Index chunk_last = i;
    for (std::size_t k = 0; k < grain && chunk_last < last; ++k) {
      ++chunk_last;
    }
    fn(i, chunk_last);
    i = chunk_last;
  }
}

}  // namespace detail
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_PARALLEL_DETAIL_GRAIN_H
