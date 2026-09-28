// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_MEMORY_RESOURCE_H_
#define BASE_MEMORY_MEMORY_RESOURCE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory_resource>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/synchronization/align.h"

namespace base {

// Format size in bytes as human-readable string (e.g. "696 B", "1.5 KiB", "2.3 GiB").
inline std::string format_bytes(size_t v) {
  char buf[32];
  const size_t kKiB = 1024ULL;
  const size_t kMiB = kKiB * 1024;
  const size_t kGiB = kMiB * 1024;
  const size_t kTiB = kGiB * 1024;
  const size_t kPiB = kTiB * 1024;
  const size_t kEiB = kPiB * 1024;
  if (v >= kEiB) {
    (void)snprintf(buf, sizeof(buf), "%.1f EiB", v / static_cast<double>(kEiB));
  } else if (v >= kPiB) {
    (void)snprintf(buf, sizeof(buf), "%.1f PiB", v / static_cast<double>(kPiB));
  } else if (v >= kTiB) {
    (void)snprintf(buf, sizeof(buf), "%.1f TiB", v / static_cast<double>(kTiB));
  } else if (v >= kGiB) {
    (void)snprintf(buf, sizeof(buf), "%.1f GiB", v / static_cast<double>(kGiB));
  } else if (v >= kMiB) {
    (void)snprintf(buf, sizeof(buf), "%.1f MiB", v / static_cast<double>(kMiB));
  } else if (v >= kKiB) {
    (void)snprintf(buf, sizeof(buf), "%.1f KiB", v / static_cast<double>(kKiB));
  } else {
    (void)snprintf(buf, sizeof(buf), "%zu B", v);
  }
  return std::string(buf);
}

// Forward declaration
class MemoryResource;
class TlsBatchedUsedBytes;

// Statistics structure for memory resources
struct MemoryResourceStatistics {
  size_t total_allocations{0};
  size_t total_deallocations{0};
  size_t peak_used{0};
  size_t current_used{0};
  size_t allocation_count{0};
  size_t deallocation_count{0};
  std::chrono::steady_clock::time_point last_reset;
};

// StatisticsManager: Helper class to manage statistics for memory resources
// Provides thread-safe statistics tracking and common operations
class StatisticsManager {
 public:
  StatisticsManager() {
    _stats.last_reset = std::chrono::steady_clock::now();
  }

  void record_allocation(size_t bytes) {
    _stats.allocation_count++;
    _stats.total_allocations += bytes;
  }

  void record_deallocation(size_t bytes) {
    _stats.deallocation_count++;
    _stats.total_deallocations += bytes;
  }

  void update_usage(size_t current_used) {
    _stats.current_used = current_used;
    if (current_used > _stats.peak_used) {
      _stats.peak_used = current_used;
    }
  }

  MemoryResourceStatistics get_statistics() const {
    return _stats;
  }

  void reset_statistics() {
    _stats.total_allocations = 0;
    _stats.total_deallocations = 0;
    _stats.peak_used = 0;
    _stats.current_used = 0;
    _stats.allocation_count = 0;
    _stats.deallocation_count = 0;
    _stats.last_reset = std::chrono::steady_clock::now();
  }

  void print_statistics(const char* class_name, const char* file, int line,
                        size_t capacity = 0) const {
    auto stats = get_statistics();
    auto now = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats.last_reset).count();

    // If there were no allocations, duration should be 0 to indicate no activity
    // Duration should represent active time, not object lifetime
    long duration = (stats.allocation_count > 0 || stats.total_allocations > 0)
                    ? duration_ms : 0;

    if (capacity > 0) {
      fprintf(stderr,
              "\033[33m [LOG_WARNING] TID [0] FUNC:[%s] [%d] [print_statistics] "
              "%s stats: "
              "total_allocations=%s, total_deallocations=%s, "
              "peak_used=%s, current_used=%s, "
              "allocation_count=%zu, deallocation_count=%zu, "
              "capacity=%s, duration=%ld ms\n",
              file, line, class_name,
              format_bytes(stats.total_allocations).c_str(),
              format_bytes(stats.total_deallocations).c_str(),
              format_bytes(stats.peak_used).c_str(),
              format_bytes(stats.current_used).c_str(),
              stats.allocation_count, stats.deallocation_count,
              format_bytes(capacity).c_str(), duration);
    } else {
      fprintf(stderr,
              "\033[33m [LOG_WARNING] TID [0] FUNC:[%s] [%d] [print_statistics] "
              "%s stats: "
              "total_allocations=%s, total_deallocations=%s, "
              "peak_used=%s, current_used=%s, "
              "allocation_count=%zu, deallocation_count=%zu, duration=%ld ms\n",
              file, line, class_name,
              format_bytes(stats.total_allocations).c_str(),
              format_bytes(stats.total_deallocations).c_str(),
              format_bytes(stats.peak_used).c_str(),
              format_bytes(stats.current_used).c_str(),
              stats.allocation_count, stats.deallocation_count, duration);
    }
  }

 private:
  mutable MemoryResourceStatistics _stats;
};

class MemoryResource : public std::pmr::memory_resource {
 public:
  static constexpr int kDefaultAlignment =
      (sizeof(void*) > 8) ? sizeof(void*) : 8;

  enum Type {
    kNewDelete = 0,
    kMonotonicBuffer,
    kUnsynchronizedPool,
    kSynchronizedPool,
    kHybridOptimized,  // Multi-tier: TLS tiny/small + synchronized medium + monotonic large
  };

  using Statistics = MemoryResourceStatistics;

  inline void clear(size_t reserved) { do_clear(reserved); }
  inline size_t capacity() const noexcept { return do_capacity(); }
  inline size_t used() const noexcept { return do_used(); }

  virtual Statistics get_statistics() const { return Statistics{}; }
  virtual void reset_statistics() {}
  virtual void set_usage_threshold(double threshold) {}
  virtual bool is_over_threshold() const { return false; }

  static MemoryResource* create(Type type, size_t size);
  static void destroy(MemoryResource* memory_resource);

 private:
  inline bool do_is_equal(const memory_resource& rhs) const noexcept override {
    return (this == &rhs);
  }

 protected:
  virtual void do_clear(size_t reserved) = 0;
  virtual size_t do_capacity() const noexcept = 0;
  virtual size_t do_used() const noexcept = 0;
};

// ThreadLocalCounter: per-thread pending delta batched into `atomic_counter`, keyed by
// the atomic's address so multiple MemoryResource instances on the same thread stay
// independent. `get` / `do_used` include pending bytes not yet flushed to the atomic.
template <typename AtomicType>
class ThreadLocalCounter {
 public:
  static constexpr size_t kBatchSize = 4096;

 private:
  // Heap-allocated map intentionally leaked at thread exit so static
  // Singleton destructors (e.g. ObjectAllocator) can still touch counters
  // after thread_local teardown — a common TLS+atexit hazard.
  static std::unordered_map<AtomicType*, std::int64_t>& pending_map() noexcept {
    thread_local std::unordered_map<AtomicType*, std::int64_t>* m =
        new std::unordered_map<AtomicType*, std::int64_t>();
    return *m;
  }

 public:
  static inline void add(AtomicType& atomic_counter, size_t bytes) noexcept {
    auto& m = pending_map();
    AtomicType* key = &atomic_counter;
    auto it = m.find(key);
    std::int64_t p =
        (it == m.end() ? 0 : it->second) + static_cast<std::int64_t>(bytes);
    if (UNLIKELY(p >= static_cast<std::int64_t>(kBatchSize))) {
      atomic_counter.fetch_add(static_cast<size_t>(p), std::memory_order_relaxed);
      p = 0;
    }
    if (p == 0) {
      m.erase(key);
    } else {
      m.insert_or_assign(key, p);
    }
  }

  static inline void subtract(AtomicType& atomic_counter, size_t bytes) noexcept {
    auto& m = pending_map();
    AtomicType* key = &atomic_counter;
    auto it = m.find(key);
    std::int64_t p =
        (it == m.end() ? 0 : it->second) - static_cast<std::int64_t>(bytes);
    if (UNLIKELY(p <= -static_cast<std::int64_t>(kBatchSize))) {
      atomic_counter.fetch_sub(static_cast<size_t>(-p), std::memory_order_relaxed);
      p = 0;
    }
    if (p == 0) {
      m.erase(key);
    } else {
      m.insert_or_assign(key, p);
    }
  }

  static inline void flush(AtomicType& atomic_counter) noexcept {
    auto& m = pending_map();
    AtomicType* key = &atomic_counter;
    auto it = m.find(key);
    if (it == m.end() || it->second == 0) {
      return;
    }
    const std::int64_t p = it->second;
    m.erase(it);
    if (p > 0) {
      atomic_counter.fetch_add(static_cast<size_t>(p), std::memory_order_relaxed);
    } else if (p < 0) {
      atomic_counter.fetch_sub(static_cast<size_t>(-p), std::memory_order_relaxed);
    }
  }

  static inline size_t get(const AtomicType& atomic_counter) noexcept {
    auto& m = pending_map();
    auto it = m.find(const_cast<AtomicType*>(&atomic_counter));
    const std::int64_t extra = (it == m.end()) ? 0 : it->second;
    const std::int64_t base =
        static_cast<std::int64_t>(atomic_counter.load(std::memory_order_relaxed));
    const std::int64_t total = base + extra;
    return total < 0 ? 0 : static_cast<size_t>(total);
  }
};

// Base class for memory resources with statistics support
// Provides common statistics management and threshold checking
class StatisticsMemoryResourceBase : public MemoryResource {
  friend class TlsBatchedUsedBytes;

 protected:
  StatisticsMemoryResourceBase() = default;

  // Common statistics methods
  Statistics get_statistics() const override {
    return _stats_manager.get_statistics();
  }

  void reset_statistics() override {
    _stats_manager.reset_statistics();
  }

  void set_usage_threshold(double threshold) override {
    _usage_threshold = threshold;
  }

  bool is_over_threshold() const override {
    if (_usage_threshold <= 0.0) return false;
    size_t cap = do_capacity();
    if (cap == SIZE_MAX || cap == 0) return false;
    auto stats = _stats_manager.get_statistics();
    return (static_cast<double>(stats.current_used) / cap) > _usage_threshold;
  }

  // Helper methods for subclasses
  void record_allocation(size_t bytes) {
    _stats_manager.record_allocation(bytes);
  }

  void record_deallocation(size_t bytes) {
    _stats_manager.record_deallocation(bytes);
  }

  void update_usage(size_t current_used) {
    _stats_manager.update_usage(current_used);
  }

  void print_statistics_on_destruct(const char* class_name) const {
    _stats_manager.print_statistics(class_name, __FILE__, __LINE__, do_capacity());
  }

  StatisticsManager _stats_manager;
  double _usage_threshold{0.0};
};

// Batches used-byte deltas (TLS → `std::atomic`) and sampled `StatisticsManager`
// updates. Used by PMR pool wrappers; also by monotonic buffers (bytes only grow
// until `reset_flush`, so deallocate uses `on_deallocate_stats_only`).
class TlsBatchedUsedBytes {
 public:
  void on_allocate_request(StatisticsMemoryResourceBase& owner,
                           size_t bytes) noexcept {
    if (UNLIKELY((++_tick & 0x3F) == 0)) {
      owner.record_allocation(bytes);
    }
    ThreadLocalCounter<std::atomic<size_t>>::add(_bytes, bytes);
  }

  void on_allocate_result(StatisticsMemoryResourceBase& owner,
                          const void* ptr) noexcept {
    if (LIKELY(ptr != nullptr) && UNLIKELY((_tick & 0x1F) == 0)) {
      owner.update_usage(ThreadLocalCounter<std::atomic<size_t>>::get(_bytes));
    }
  }

  void on_deallocate_request(StatisticsMemoryResourceBase& owner,
                             size_t bytes) noexcept {
    if (UNLIKELY((++_tick & 0x3F) == 0)) {
      owner.record_deallocation(bytes);
    }
    ThreadLocalCounter<std::atomic<size_t>>::subtract(_bytes, bytes);
  }

  void on_deallocate_after_backend(StatisticsMemoryResourceBase& owner) noexcept {
    if (UNLIKELY((_tick & 0x1F) == 0)) {
      owner.update_usage(ThreadLocalCounter<std::atomic<size_t>>::get(_bytes));
    }
  }

  // Monotonic (and similar) resources: sample deallocation stats without shrinking
  // the cumulative used-byte counter.
  void on_deallocate_stats_only(StatisticsMemoryResourceBase& owner,
                                size_t bytes) noexcept {
    if (UNLIKELY((++_tick & 0x3F) == 0)) {
      owner.record_deallocation(bytes);
    }
  }

  void reset_flush() noexcept {
    ThreadLocalCounter<std::atomic<size_t>>::flush(_bytes);
    _bytes.store(0, std::memory_order_relaxed);
    _tick.store(0, std::memory_order_relaxed);
  }

  size_t used() const noexcept {
    return ThreadLocalCounter<std::atomic<size_t>>::get(_bytes);
  }

 private:
  alignas(hardware_destructive_interference_size) std::atomic<size_t> _bytes{};
  alignas(hardware_destructive_interference_size) std::atomic<uint64_t> _tick{0};
};

class NewDeleteMemoryResource : public StatisticsMemoryResourceBase {
 public:
  NewDeleteMemoryResource() { _impl = std::pmr::new_delete_resource(); }
  ~NewDeleteMemoryResource() override {
    print_statistics_on_destruct("NewDeleteMemoryResource");
  }

 protected:
  void* do_allocate(size_t bytes, size_t align) override {
    _usage.on_allocate_request(*this, bytes);
    void* ptr = _impl->allocate(bytes, align);
    _usage.on_allocate_result(*this, ptr);
    return ptr;
  }
  void do_deallocate(void* p, size_t bytes, size_t align) override {
    _usage.on_deallocate_request(*this, bytes);
    _impl->deallocate(p, bytes, align);
    _usage.on_deallocate_after_backend(*this);
  }
  void do_clear(size_t reserved) override {
    (void)reserved;
    _usage.reset_flush();
    reset_statistics();
  }

  size_t do_capacity() const noexcept override { return SIZE_MAX; }
  size_t do_used() const noexcept override { return _usage.used(); }

  std::pmr::memory_resource* _impl{};
  TlsBatchedUsedBytes _usage;
};

class MonotonicBufferMemoryResource : public StatisticsMemoryResourceBase {
 public:
  MonotonicBufferMemoryResource(size_t size) : _buffer(size > 0 ? size : 1024 * 1024) {
    new (&_impl) std::pmr::monotonic_buffer_resource(
        _buffer.data(), _buffer.size(), std::pmr::new_delete_resource());
  }
  ~MonotonicBufferMemoryResource() override {
    _impl.~monotonic_buffer_resource();
    print_statistics_on_destruct("MonotonicBufferMemoryResource");
  }

 protected:
  void* do_allocate(size_t bytes, size_t align) override {
    _usage.on_allocate_request(*this, bytes);
    void* ptr = _impl.allocate(bytes, align);
    _usage.on_allocate_result(*this, ptr);
    return ptr;
  }
  void do_deallocate(void* p, size_t bytes, size_t align) override {
    // Cumulative `used()` is reset only in `do_clear`; PMR deallocate is a no-op for bytes.
    _usage.on_deallocate_stats_only(*this, bytes);
    _impl.deallocate(p, bytes, align);
  }
  void do_clear(size_t reserved) override {
    _usage.reset_flush();
    if (reserved > _buffer.size()) {
      _buffer.resize(reserved);
    }

    _impl.~monotonic_buffer_resource();
    new (&_impl) std::pmr::monotonic_buffer_resource(
        _buffer.data(), _buffer.size(), std::pmr::new_delete_resource());
    reset_statistics();
  }

  size_t do_capacity() const noexcept override {
    return _buffer.size();
  }
  size_t do_used() const noexcept override { return _usage.used(); }

 private:
  std::vector<char> _buffer;
  std::pmr::monotonic_buffer_resource _impl;
  TlsBatchedUsedBytes _usage;
};

class UnsynchronizedPoolMemoryResource : public StatisticsMemoryResourceBase {
 public:
  /// `size` is accepted for factory/API compatibility; the pool uses the default upstream.
  explicit UnsynchronizedPoolMemoryResource(size_t size) : _upstream(nullptr) {
    (void)size;
  }

  /// Session-scoped pool: allocations fall back to `upstream` (e.g. app `MemoryResource`).
  explicit UnsynchronizedPoolMemoryResource(std::pmr::memory_resource* upstream)
      : _upstream(upstream) {
    if (upstream) {
      _impl.~unsynchronized_pool_resource();
      new (&_impl)::std::pmr::unsynchronized_pool_resource(upstream);
    }
  }

  ~UnsynchronizedPoolMemoryResource() override {
    print_statistics_on_destruct("UnsynchronizedPoolMemoryResource");
  }

  void release() { _impl.release(); }

 protected:
  void* do_allocate(size_t bytes, size_t align) override {
    _usage.on_allocate_request(*this, bytes);
    void* ptr = _impl.allocate(bytes, align);
    _usage.on_allocate_result(*this, ptr);
    return ptr;
  }
  void do_deallocate(void* p, size_t bytes, size_t align) override {
    _usage.on_deallocate_request(*this, bytes);
    _impl.deallocate(p, bytes, align);
    _usage.on_deallocate_after_backend(*this);
  }
  void do_clear(size_t reserved) override {
    (void)reserved;
    _usage.reset_flush();
    _impl.~unsynchronized_pool_resource();
    if (_upstream) {
      new (&_impl)::std::pmr::unsynchronized_pool_resource(_upstream);
    } else {
      new (&_impl)::std::pmr::unsynchronized_pool_resource();
    }
    reset_statistics();
  }

  size_t do_capacity() const noexcept override { return SIZE_MAX; }
  size_t do_used() const noexcept override { return _usage.used(); }

 private:
  std::pmr::memory_resource* _upstream;
  std::pmr::unsynchronized_pool_resource _impl;
  TlsBatchedUsedBytes _usage;
};

class SynchronizedPoolMemoryResource : public StatisticsMemoryResourceBase {
 public:
  explicit SynchronizedPoolMemoryResource(size_t /*chunk_hint*/) {}
  ~SynchronizedPoolMemoryResource() override {
    print_statistics_on_destruct("SynchronizedPoolMemoryResource");
  }

 protected:
  void* do_allocate(size_t bytes, size_t align) override {
    _usage.on_allocate_request(*this, bytes);
    void* ptr = _impl.allocate(bytes, align);
    _usage.on_allocate_result(*this, ptr);
    return ptr;
  }
  void do_deallocate(void* p, size_t bytes, size_t align) override {
    _usage.on_deallocate_request(*this, bytes);
    _impl.deallocate(p, bytes, align);
    _usage.on_deallocate_after_backend(*this);
  }
  void do_clear(size_t reserved) override {
    (void)reserved;
    _usage.reset_flush();
    _impl.~synchronized_pool_resource();
    new (&_impl)::std::pmr::synchronized_pool_resource();
    reset_statistics();
  }

  size_t do_capacity() const noexcept override { return SIZE_MAX; }
  size_t do_used() const noexcept override { return _usage.used(); }

 private:
  std::pmr::synchronized_pool_resource _impl;
  TlsBatchedUsedBytes _usage;
};

// HybridOptimizedMemoryResource: Multi-tier PMR (merged from kHybridOptimized + kHighPerformance)
// - Tiny (< 64B): Thread-local unsynchronized pool (zero contention)
// - Small (64B - 256B): Thread-local unsynchronized pool
// - Medium (256B - 4KB): Synchronized pool
// - Large (> 4KB): Monotonic buffer
class HybridOptimizedMemoryResource : public StatisticsMemoryResourceBase {
 public:
  explicit HybridOptimizedMemoryResource(size_t initial_size = 4 * 1024 * 1024)
      : _buffer_size(initial_size),
        _buffer(initial_size > 0 ? new char[initial_size] : nullptr),
        _monotonic_buffer(_buffer.get(), _buffer_size, std::pmr::new_delete_resource()),
        _medium_pool(&_monotonic_buffer) {
    _size.store(0, std::memory_order_relaxed);
    initialize_tls_pools();
  }

  ~HybridOptimizedMemoryResource() override {
    if (_buffer_size > 0) {
      print_statistics_on_destruct("HybridOptimizedMemoryResource");
    }
    auto& pools = get_tls_pools();
    if (pools.tiny_pool) {
      delete pools.tiny_pool;
      pools.tiny_pool = nullptr;
    }
    if (pools.small_pool) {
      delete pools.small_pool;
      pools.small_pool = nullptr;
    }
  }

 protected:
  void* do_allocate(size_t bytes, size_t align) override {
    if (UNLIKELY((++_stat_counter & 0x3F) == 0)) {
      record_allocation(bytes);
    }

    void* ptr = nullptr;
    if (LIKELY(bytes <= kTinyObjectThreshold)) {
      ptr = tls_pool_allocate(get_tls_pools().tiny_pool, bytes, align);
    } else if (UNLIKELY(bytes <= kSmallObjectThreshold)) {
      ptr = tls_pool_allocate(get_tls_pools().small_pool, bytes, align);
    } else if (UNLIKELY(bytes <= kMediumObjectThreshold)) {
      ptr = _medium_pool.allocate(bytes, align);
    } else {
      ptr = _monotonic_buffer.allocate(bytes, align);
    }

    if (LIKELY(ptr != nullptr)) {
      ThreadLocalCounter<decltype(_size)>::add(_size, bytes);
      if (UNLIKELY((_stat_counter & 0x1F) == 0)) {
        update_usage(ThreadLocalCounter<decltype(_size)>::get(_size));
      }
    }

    return ptr;
  }

  void do_deallocate(void* p, size_t bytes, size_t align) override {
    if (UNLIKELY((++_stat_counter & 0x3F) == 0)) {
      record_deallocation(bytes);
    }

    auto& pools = get_tls_pools();
    if (LIKELY(bytes <= kTinyObjectThreshold)) {
      tls_pool_deallocate_or_fallback(pools.tiny_pool, p, bytes, align);
    } else if (UNLIKELY(bytes <= kSmallObjectThreshold)) {
      tls_pool_deallocate_or_fallback(pools.small_pool, p, bytes, align);
    } else if (UNLIKELY(bytes <= kMediumObjectThreshold)) {
      _medium_pool.deallocate(p, bytes, align);
    }
    // Large objects in monotonic buffer are not individually deallocated

    ThreadLocalCounter<decltype(_size)>::subtract(_size, bytes);
    if (UNLIKELY((_stat_counter & 0x1F) == 0)) {
      update_usage(ThreadLocalCounter<decltype(_size)>::get(_size));
    }
  }

  void do_clear(size_t reserved) override {
    ThreadLocalCounter<decltype(_size)>::flush(_size);
    _size.store(0, std::memory_order_relaxed);

    size_t new_size = (std::max)(reserved, size_t(1024));
    if (new_size > _buffer_size) {
      _buffer_size = new_size;
      _buffer.reset(new char[_buffer_size]);
    }

    _monotonic_buffer.~monotonic_buffer_resource();
    new (&_monotonic_buffer)
        std::pmr::monotonic_buffer_resource(_buffer.get(), _buffer_size,
                                            std::pmr::new_delete_resource());

    _medium_pool.~synchronized_pool_resource();
    new (&_medium_pool) std::pmr::synchronized_pool_resource(&_monotonic_buffer);

    auto& pools = get_tls_pools();
    if (pools.tiny_pool) {
      pools.tiny_pool->release();
    }
    if (pools.small_pool) {
      pools.small_pool->release();
    }

    reset_statistics();
  }

  size_t do_capacity() const noexcept override { return _buffer_size; }

  size_t do_used() const noexcept override {
    return ThreadLocalCounter<decltype(_size)>::get(_size);
  }

 private:
  struct ThreadLocalPools {
    std::pmr::unsynchronized_pool_resource* tiny_pool = nullptr;
    std::pmr::unsynchronized_pool_resource* small_pool = nullptr;
  };

  static ThreadLocalPools& get_tls_pools() {
    thread_local static ThreadLocalPools pools;
    return pools;
  }

  void* tls_pool_allocate(std::pmr::unsynchronized_pool_resource*& slot, size_t bytes,
                          size_t align) {
    if (LIKELY(slot != nullptr)) {
      return slot->allocate(bytes, align);
    }
    slot = new std::pmr::unsynchronized_pool_resource(&_medium_pool);
    return slot->allocate(bytes, align);
  }

  void tls_pool_deallocate_or_fallback(std::pmr::unsynchronized_pool_resource* slot,
                                       void* p, size_t bytes, size_t align) {
    if (LIKELY(slot != nullptr)) {
      slot->deallocate(p, bytes, align);
    } else {
      _medium_pool.deallocate(p, bytes, align);
    }
  }

  void initialize_tls_pools() {
    auto& pools = get_tls_pools();
    if (!pools.tiny_pool) {
      pools.tiny_pool = new std::pmr::unsynchronized_pool_resource(&_medium_pool);
    }
    if (!pools.small_pool) {
      pools.small_pool = new std::pmr::unsynchronized_pool_resource(&_medium_pool);
    }
  }

  static constexpr size_t kTinyObjectThreshold = 64;
  static constexpr size_t kSmallObjectThreshold = 256;
  static constexpr size_t kMediumObjectThreshold = 4 * 1024;

  alignas(hardware_destructive_interference_size) std::atomic<uint64_t> _stat_counter{0};

  size_t _buffer_size;
  std::unique_ptr<char[]> _buffer;
  std::pmr::monotonic_buffer_resource _monotonic_buffer;
  std::pmr::synchronized_pool_resource _medium_pool;
  alignas(hardware_destructive_interference_size) std::atomic<size_t> _size;
};

inline MemoryResource* MemoryResource::create(MemoryResource::Type type, size_t size) {
  switch (type) {
    case kNewDelete:
      return new NewDeleteMemoryResource();
    case kMonotonicBuffer:
      return new MonotonicBufferMemoryResource(size);
    case kUnsynchronizedPool:
      return new UnsynchronizedPoolMemoryResource(size);
    case kSynchronizedPool:
      return new SynchronizedPoolMemoryResource(size);
    case kHybridOptimized:
      return new HybridOptimizedMemoryResource(size);
    default:
      return new NewDeleteMemoryResource();
  }
}

inline void MemoryResource::destroy(MemoryResource* memory_resource) {
  delete memory_resource;
}

}  // namespace base
#endif  // BASE_MEMORY_MEMORY_RESOURCE_H_

