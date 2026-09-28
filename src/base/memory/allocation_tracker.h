// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_ALLOCATION_TRACKER_H_
#define BASE_MEMORY_ALLOCATION_TRACKER_H_

#include <chrono>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "base/core/log.h"
#include "base/memory/arena.h"

namespace base {
class AllocationTracker {
 public:
  struct AllocationInfo {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    std::chrono::steady_clock::time_point timestamp;
  };

  static void track_allocation(void* ptr, size_t size, const char* file, int line) {
    if (!enabled_.load(std::memory_order_relaxed)) {
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    AllocationInfo info{ptr, size, file, line,
                        std::chrono::steady_clock::now()};
    allocations_[ptr] = info;
    total_allocated_.fetch_add(size, std::memory_order_relaxed);
    allocation_count_.fetch_add(1, std::memory_order_relaxed);
    update_peak_size_locked();
  }

  static void track_deallocation(void* ptr) {
    if (!enabled_.load(std::memory_order_relaxed)) {
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
      total_deallocated_.fetch_add(it->second.size, std::memory_order_relaxed);
      deallocation_count_.fetch_add(1, std::memory_order_relaxed);
      allocations_.erase(it);
    }
  }

  static void dump_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t live_count = allocations_.size();
    size_t live_size = 0;
    for (const auto& [ptr, info] : allocations_) {
      live_size += info.size;
    }

    LOGGING(LOG_INFO, "=== Memory Allocation Statistics ===");
    LOGGING(LOG_INFO, "Total allocations: %lu", allocation_count_.load());
    LOGGING(LOG_INFO, "Total deallocations: %lu", deallocation_count_.load());
    LOGGING(LOG_INFO, "Live allocations: %lu", live_count);
    LOGGING(LOG_INFO, "Total allocated: %lu bytes", total_allocated_.load());
    LOGGING(LOG_INFO, "Total deallocated: %lu bytes", total_deallocated_.load());
    LOGGING(LOG_INFO, "Live size: %lu bytes", live_size);
    LOGGING(LOG_INFO, "Peak size: %lu bytes", peak_size_.load());

    if (live_count > 0) {
      LOGGING(LOG_INFO, "=== Potential Leaks ===");
      size_t reported = 0;
      for (const auto& [ptr, info] : allocations_) {
        if (reported++ < 10) {
          LOGGING(LOG_INFO, "Leak: %p (%lu bytes) at %s:%d", ptr, info.size,
                  info.file, info.line);
        }
      }
      if (live_count > 10) {
        LOGGING(LOG_INFO, "... and %lu more", live_count - 10);
      }
    }
  }

  static void enable() { enabled_.store(true, std::memory_order_relaxed); }
  static void disable() { enabled_.store(false, std::memory_order_relaxed); }
  static bool is_enabled() { return enabled_.load(std::memory_order_relaxed); }

  static size_t peak_bytes() {
    return peak_size_.load(std::memory_order_relaxed);
  }
  static size_t live_bytes() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t live_size = 0;
    for (const auto& [ptr, info] : allocations_) {
      live_size += info.size;
    }
    return live_size;
  }
  static size_t live_count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.size();
  }

  static void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    allocations_.clear();
    total_allocated_.store(0, std::memory_order_relaxed);
    total_deallocated_.store(0, std::memory_order_relaxed);
    allocation_count_.store(0, std::memory_order_relaxed);
    deallocation_count_.store(0, std::memory_order_relaxed);
    peak_size_.store(0, std::memory_order_relaxed);
  }

  static std::vector<AllocationInfo> get_live_allocations() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AllocationInfo> result;
    result.reserve(allocations_.size());
    for (const auto& [ptr, info] : allocations_) {
      result.push_back(info);
    }
    return result;
  }

 private:
  inline static std::mutex mutex_{};
  inline static std::unordered_map<void*, AllocationInfo> allocations_{};
  inline static std::atomic<size_t> total_allocated_{0};
  inline static std::atomic<size_t> total_deallocated_{0};
  inline static std::atomic<size_t> allocation_count_{0};
  inline static std::atomic<size_t> deallocation_count_{0};
  inline static std::atomic<size_t> peak_size_{0};
  inline static std::atomic<bool> enabled_{false};

  static void update_peak_size_locked() {
    size_t current_live = 0;
    for (const auto& [ptr, info] : allocations_) {
      current_live += info.size;
    }
    size_t current_peak = peak_size_.load(std::memory_order_relaxed);
    while (current_live > current_peak) {
      if (peak_size_.compare_exchange_weak(current_peak, current_live,
                                           std::memory_order_relaxed)) {
        break;
      }
    }
  }
};


inline void* tracked_allocate_impl(size_t size, const char* file, int line) {
  void* ptr = base::allocate(size);
  if (ptr) {
    base::AllocationTracker::track_allocation(ptr, size, file, line);
  }
  return ptr;
}

#define TRACKED_ALLOCATE(size) \
  base::tracked_allocate_impl(size, __FILE__, __LINE__)

#define TRACKED_DEALLOCATE(ptr, size)                                       \
  do {                                                                       \
    if (ptr) {                                                               \
      base::AllocationTracker::track_deallocation(ptr);                     \
      base::deallocate(ptr, size);                                           \
    }                                                                        \
  } while (0)

}  // namespace base
#endif  // BASE_MEMORY_ALLOCATION_TRACKER_H_

