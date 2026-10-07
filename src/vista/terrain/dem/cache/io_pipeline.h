// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CACHE_IO_PIPELINE_H_
#define VISTA_TERRAIN_DEM_CACHE_IO_PIPELINE_H_

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace vista {
namespace detail {

// Warm-cache reads through 64 MiB were faster as one ReadFile than as
// mmap plus a parallel memcpy. Keep the pool for larger payloads only.
inline constexpr size_t kDemIoMinParallelBytes = 128u << 20;  // 128 MiB

inline void io_pause() {
#if defined(_MSC_VER)
  _mm_pause();
#else
  std::this_thread::yield();
#endif
}

// Contiguous [begin, end) for |part| in [0, nparts). Interior cuts are
// aligned down to |grain| (power of two) so adjacent parts do not overlap.
inline void io_part_range(size_t n, int nparts, int part, size_t grain,
                          size_t* begin, size_t* end) {
  auto cut = [&](int p) -> size_t {
    if (p <= 0) {
      return 0;
    }
    if (p >= nparts) {
      return n;
    }
    size_t x = (n * static_cast<size_t>(p)) / static_cast<size_t>(nparts);
    if (grain > 1) {
      x &= ~(grain - 1);
    }
    return x;
  };
  *begin = cut(part);
  *end = cut(part + 1);
  if (*end < *begin) {
    *end = *begin;
  }
}

// Persistent pool. One in-flight job; caller is the last part. Each
// participant runs one contiguous range (no per-chunk steal, no
// std::function). Not re-entrant.
class DemIoPool {
 public:
  using PartFn = void (*)(void* ctx, size_t begin, size_t end);

  static DemIoPool& get() {
    static DemIoPool pool;
    return pool;
  }

  void execute(size_t n, size_t grain, PartFn fn, void* ctx) {
    if (n == 0 || !fn) {
      return;
    }
    if (thread_count_ == 0) {
      fn(ctx, 0, n);
      return;
    }

    std::unique_lock<std::mutex> lock(mu_);
    cv_idle_.wait(lock, [&] { return !busy_; });
    busy_ = true;
    fn_ = fn;
    ctx_ = ctx;
    n_ = n;
    grain_ = grain == 0 ? 1 : grain;
    nparts_ = thread_count_ + 1;
    finished_.store(0, std::memory_order_relaxed);
    generation_.fetch_add(1, std::memory_order_release);
    lock.unlock();
    cv_work_.notify_all();

    run_part(thread_count_);

    for (int spins = 0; spins < 8192; ++spins) {
      if (finished_.load(std::memory_order_acquire) == thread_count_) {
        goto done;
      }
      io_pause();
    }
    {
      std::unique_lock<std::mutex> wait_lock(mu_);
      cv_done_.wait(wait_lock, [&] {
        return finished_.load(std::memory_order_acquire) == thread_count_;
      });
    }
  done:
    lock.lock();
    fn_ = nullptr;
    busy_ = false;
    lock.unlock();
    cv_idle_.notify_one();
  }

  DemIoPool(const DemIoPool&) = delete;
  DemIoPool& operator=(const DemIoPool&) = delete;

 private:
  DemIoPool() {
    const unsigned hw = std::thread::hardware_concurrency();
    if (hw <= 1) {
      return;
    }
    thread_count_ = static_cast<int>(hw - 1);
    threads_.reserve(static_cast<size_t>(thread_count_));
    for (int i = 0; i < thread_count_; ++i) {
      threads_.emplace_back([this, i] { worker_main(i); });
    }
  }

  ~DemIoPool() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      shutdown_.store(true, std::memory_order_release);
      fn_ = nullptr;
      generation_.fetch_add(1, std::memory_order_release);
    }
    cv_work_.notify_all();
    cv_done_.notify_all();
    cv_idle_.notify_all();
    for (std::thread& t : threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  void run_part(int part) {
    PartFn fn = fn_;
    void* ctx = ctx_;
    if (!fn) {
      return;
    }
    size_t begin = 0;
    size_t end = 0;
    io_part_range(n_, nparts_, part, grain_, &begin, &end);
    if (begin < end) {
      fn(ctx, begin, end);
    }
  }

  void worker_main(int id) {
    size_t seen = 0;
    for (;;) {
      size_t g = generation_.load(std::memory_order_acquire);
      int spins = 0;
      while (g == seen && !shutdown_.load(std::memory_order_acquire)) {
        if (spins++ < 4096) {
          io_pause();
          g = generation_.load(std::memory_order_acquire);
          continue;
        }
        std::unique_lock<std::mutex> lock(mu_);
        cv_work_.wait(lock, [&] {
          return shutdown_.load(std::memory_order_acquire) ||
                 generation_.load(std::memory_order_acquire) != seen;
        });
        g = generation_.load(std::memory_order_acquire);
        break;
      }
      if (shutdown_.load(std::memory_order_acquire)) {
        return;
      }
      seen = g;
      run_part(id);
      const int prev = finished_.fetch_add(1, std::memory_order_acq_rel);
      if (prev + 1 == thread_count_) {
        std::lock_guard<std::mutex> lock(mu_);
        cv_done_.notify_one();
      }
    }
  }

  std::mutex mu_;
  std::condition_variable cv_work_;
  std::condition_variable cv_done_;
  std::condition_variable cv_idle_;
  PartFn fn_ = nullptr;
  void* ctx_ = nullptr;
  size_t n_ = 0;
  size_t grain_ = 1;
  int nparts_ = 1;
  std::atomic<size_t> generation_{0};
  std::atomic<int> finished_{0};
  int thread_count_ = 0;
  bool busy_ = false;
  std::atomic<bool> shutdown_{false};
  std::vector<std::thread> threads_;
};

// Run |prepare_at|(index) for index in [0, job_count). Each worker owns one
// contiguous index span.
template <typename PrepareAt>
void run_chunked_io_pipeline(size_t job_count, PrepareAt&& prepare_at) {
  if (job_count == 0) {
    return;
  }
  if (job_count == 1) {
    prepare_at(0);
    return;
  }
  struct Ctx {
    PrepareAt* fn;
  } ctx{&prepare_at};
  DemIoPool::get().execute(
      job_count, /*grain=*/1,
      [](void* p, size_t begin, size_t end) {
        auto& fn = *static_cast<Ctx*>(p)->fn;
        for (size_t i = begin; i < end; ++i) {
          fn(i);
        }
      },
      &ctx);
}

struct DemIoCopy {
  uint8_t* dst;
  const uint8_t* src;
};

inline void copy_bytes_part(void* p, size_t begin, size_t end) {
  auto* c = static_cast<DemIoCopy*>(p);
  std::memcpy(c->dst + begin, c->src + begin, end - begin);
}

// Parallel memcpy for large payloads. Below the threshold, one serial copy.
inline void copy_bytes_chunked(void* dst, const void* src, size_t bytes) {
  if (!dst || !src || bytes == 0) {
    return;
  }
  if (bytes < kDemIoMinParallelBytes) {
    std::memcpy(dst, src, bytes);
    return;
  }
  DemIoCopy job{static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src)};
  DemIoPool::get().execute(bytes, /*grain=*/64, &copy_bytes_part, &job);
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CACHE_IO_PIPELINE_H_
