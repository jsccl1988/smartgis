// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_ALLOCATION_TRACKER_H_
#define BASE_MEMORY_ALLOCATION_TRACKER_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/core/export.h"
#include "base/core/log.h"
#include "base/memory/arena.h"

namespace base {
class AllocationTracker {
 public:
  struct AllocationInfo {
    static constexpr int kStackMax = 12;

    void* ptr;
    size_t size;
    const char* file;
    int line;
    std::chrono::steady_clock::time_point timestamp;
    // Outermost frame is frames[depth - 1]. type is interned (stable).
    void* frames[kStackMax]{};
    int depth = 0;
    const char* type = nullptr;
  };

  // One flame-graph node. type is an interned object name; pc is a caller.
  struct FlameNode {
    void* pc = nullptr;
    const char* type = nullptr;
    uint64_t bytes = 0;
    uint32_t count = 0;
    std::vector<FlameNode> children;
  };

  // Copy |name| into process-lifetime storage. Empty → nullptr.
  static const char* intern_type_name(std::string_view name) {
    if (name.empty()) {
      return nullptr;
    }
    std::lock_guard<std::mutex> lock(state().mutex);
    return intern_locked(name);
  }

  // Live allocations merged by call stack, with the object type as the leaf.
  // Root (single element) has no pc/type; children are outermost frames.
  static std::vector<FlameNode> flame_snapshot();

  static void track_allocation(void* ptr, size_t size, const char* file, int line,
                              const char* type = nullptr) {
    State& s = state();
    if (!s.enabled.load(std::memory_order_relaxed)) {
      return;
    }

    std::lock_guard<std::mutex> lock(s.mutex);
    AllocationInfo info{ptr, size, file, line,
                        std::chrono::steady_clock::now()};
    info.depth = capture_alloc_stack(info.frames, AllocationInfo::kStackMax);
    info.type = type ? type : site_label_locked(file, line);
    s.allocations[ptr] = info;
    s.total_allocated.fetch_add(size, std::memory_order_relaxed);
    s.allocation_count.fetch_add(1, std::memory_order_relaxed);
    update_peak_size_locked();
  }

  static void track_deallocation(void* ptr) {
    State& s = state();
    if (!s.enabled.load(std::memory_order_relaxed)) {
      return;
    }

    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.allocations.find(ptr);
    if (it != s.allocations.end()) {
      s.total_deallocated.fetch_add(it->second.size, std::memory_order_relaxed);
      s.deallocation_count.fetch_add(1, std::memory_order_relaxed);
      s.allocations.erase(it);
    }
  }

  static void dump_statistics() {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);

    size_t live_count = s.allocations.size();
    size_t live_size = 0;
    for (const auto& [ptr, info] : s.allocations) {
      live_size += info.size;
    }

    LOGGING(LOG_INFO, "=== Memory Allocation Statistics ===");
    LOGGING(LOG_INFO, "Total allocations: %lu", s.allocation_count.load());
    LOGGING(LOG_INFO, "Total deallocations: %lu", s.deallocation_count.load());
    LOGGING(LOG_INFO, "Live allocations: %lu", live_count);
    LOGGING(LOG_INFO, "Total allocated: %lu bytes", s.total_allocated.load());
    LOGGING(LOG_INFO, "Total deallocated: %lu bytes", s.total_deallocated.load());
    LOGGING(LOG_INFO, "Live size: %lu bytes", live_size);
    LOGGING(LOG_INFO, "Peak size: %lu bytes", s.peak_size.load());

    if (live_count > 0) {
      LOGGING(LOG_INFO, "=== Potential Leaks ===");
      size_t reported = 0;
      for (const auto& [ptr, info] : s.allocations) {
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

  static void enable() {
    state().enabled.store(true, std::memory_order_relaxed);
  }
  static void disable() {
    state().enabled.store(false, std::memory_order_relaxed);
  }
  static bool is_enabled() {
    return state().enabled.load(std::memory_order_relaxed);
  }

  static size_t peak_bytes() {
    return state().peak_size.load(std::memory_order_relaxed);
  }
  static size_t live_bytes() {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    size_t live_size = 0;
    for (const auto& [ptr, info] : s.allocations) {
      live_size += info.size;
    }
    return live_size;
  }
  static size_t live_count() {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.allocations.size();
  }

  static void reset() {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.allocations.clear();
    s.total_allocated.store(0, std::memory_order_relaxed);
    s.total_deallocated.store(0, std::memory_order_relaxed);
    s.allocation_count.store(0, std::memory_order_relaxed);
    s.deallocation_count.store(0, std::memory_order_relaxed);
    s.peak_size.store(0, std::memory_order_relaxed);
  }

  static std::vector<AllocationInfo> get_live_allocations() {
    State& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    std::vector<AllocationInfo> result;
    result.reserve(s.allocations.size());
    for (const auto& [ptr, info] : s.allocations) {
      result.push_back(info);
    }
    return result;
  }

 private:
  // One process-wide table. Inline statics used to be a copy per DLL, so
  // base::create in content/gis never showed up in the Views memory tab.
  struct State {
    std::mutex mutex;
    std::unordered_map<void*, AllocationInfo> allocations;
    std::atomic<size_t> total_allocated{0};
    std::atomic<size_t> total_deallocated{0};
    std::atomic<size_t> allocation_count{0};
    std::atomic<size_t> deallocation_count{0};
    std::atomic<size_t> peak_size{0};
    std::atomic<bool> enabled{false};
    std::unordered_map<std::string, const char*> intern_index;
    std::vector<std::unique_ptr<std::string>> intern_store;
  };

  BASE_EXPORT static State& state();

  static int capture_alloc_stack(void** frames, int max_frames);
  static const char* intern_locked(std::string_view name);
  static const char* site_label_locked(const char* file, int line);

  static void update_peak_size_locked() {
    State& s = state();
    size_t current_live = 0;
    for (const auto& [ptr, info] : s.allocations) {
      current_live += info.size;
    }
    size_t current_peak = s.peak_size.load(std::memory_order_relaxed);
    while (current_live > current_peak) {
      if (s.peak_size.compare_exchange_weak(current_peak, current_live,
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

