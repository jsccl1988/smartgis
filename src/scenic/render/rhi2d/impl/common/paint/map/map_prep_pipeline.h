// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_PREP_PIPELINE_H_
#define SCENIC_RHI2D_MAP_PREP_PIPELINE_H_

#include <algorithm>
#include <atomic>
#include <thread>
#include <utility>
#include <vector>

namespace scenic {
namespace detail {

// Parallel CPU prep (LP→DP + thin) then serial GDI play. HDC stays
// single-thread.
inline constexpr size_t kMinParallelFeatures = 16;
// Steal granularity: one chunk covers many features so spawn/join does not
// dominate china-scale layers (1k–3k feats).
inline constexpr size_t kPrepChunk = 48;

// Run prepare_at(i) for i in [0, job_count). Lock-free chunk steal: no
// Pipeline queues, no mutex freelist, no heap Context*. Caller thread is a
// worker. Debug and Release use the same path.
template <typename PrepareAt>
void run_chunked_prep_pipeline(size_t job_count, PrepareAt&& prepare_at) {
  if (job_count == 0) {
    return;
  }
  const size_t n_chunks = (job_count + kPrepChunk - 1) / kPrepChunk;
  const size_t hw = static_cast<size_t>(
      (std::max)(1u, std::thread::hardware_concurrency()));
  const size_t workers =
      (std::max)(size_t{1}, (std::min)(n_chunks, hw));

  std::atomic<size_t> next_chunk{0};
  auto steal = [&]() {
    for (;;) {
      const size_t c = next_chunk.fetch_add(1, std::memory_order_relaxed);
      if (c >= n_chunks) {
        return;
      }
      const size_t begin = c * kPrepChunk;
      const size_t end = (std::min)(job_count, begin + kPrepChunk);
      for (size_t i = begin; i < end; ++i) {
        prepare_at(i);
      }
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(workers > 0 ? workers - 1 : 0);
  struct JoinPool {
    std::vector<std::thread>* threads = nullptr;
    ~JoinPool() {
      if (!threads) {
        return;
      }
      for (std::thread& t : *threads) {
        if (t.joinable()) {
          t.join();
        }
      }
    }
  } join{&pool};

  for (size_t w = 1; w < workers; ++w) {
    pool.emplace_back(steal);
  }
  steal();
}

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_PREP_PIPELINE_H_
