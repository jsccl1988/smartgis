// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CACHE_IO_PIPELINE_H_
#define VISTA_TERRAIN_DEM_CACHE_IO_PIPELINE_H_

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace vista {
namespace detail {

// Steal granularity for large cache blob copies (heights / RGBA / mesh).
// Mirrors scenic map_prep_pipeline: chunked work so spawn/join does not
// dominate modest payloads.
inline constexpr size_t kDemIoChunkBytes = 256u * 1024u;
inline constexpr size_t kDemIoMinParallelBytes = 512u * 1024u;

// Run |prepare_at|(chunk_index) for chunk_index in [0, job_count). Lock-free
// chunk steal (caller thread is a worker) — same shape as
// scenic::detail::run_chunked_prep_pipeline.
template <typename PrepareAt>
void run_chunked_io_pipeline(size_t job_count, PrepareAt&& prepare_at) {
  if (job_count == 0) {
    return;
  }
  const size_t hw = static_cast<size_t>(
      (std::max)(1u, std::thread::hardware_concurrency()));
  const size_t workers = (std::max)(size_t{1}, (std::min)(job_count, hw));

  std::atomic<size_t> next{0};
  auto steal = [&]() {
    for (;;) {
      const size_t c = next.fetch_add(1, std::memory_order_relaxed);
      if (c >= job_count) {
        return;
      }
      prepare_at(c);
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

// Parallel memcpy for large cache payloads; small copies stay serial.
inline void copy_bytes_chunked(void* dst, const void* src, size_t bytes) {
  if (!dst || !src || bytes == 0) {
    return;
  }
  if (bytes < kDemIoMinParallelBytes) {
    std::memcpy(dst, src, bytes);
    return;
  }
  auto* d = static_cast<uint8_t*>(dst);
  const auto* s = static_cast<const uint8_t*>(src);
  const size_t n_chunks = (bytes + kDemIoChunkBytes - 1) / kDemIoChunkBytes;
  run_chunked_io_pipeline(n_chunks, [&](size_t c) {
    const size_t begin = c * kDemIoChunkBytes;
    const size_t n = (std::min)(kDemIoChunkBytes, bytes - begin);
    std::memcpy(d + begin, s + begin, n);
  });
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CACHE_IO_PIPELINE_H_
