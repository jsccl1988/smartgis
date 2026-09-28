// Copyright (c) 2023 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONCURRENCY_CONCURRENT_H
#define BASE_CONCURRENCY_CONCURRENT_H

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>

#include "base/memory/noncopyable.h"

namespace base {
template <typename T>
concept basic_lockable = requires(T a) {
  { a.lock() } -> std::same_as<void>;
  { a.unlock() } -> std::same_as<void>;
};

template <typename T>
concept lockable = basic_lockable<T> && requires(T a) {
  { a.try_lock() } -> std::same_as<bool>;
};

template <typename T>
concept timed_lockable = lockable<T> && requires(T a) {
  { a.try_lock_for(std::chrono::seconds{}) } -> std::same_as<bool>;

  {
    a.try_lock_until(std::chrono::time_point<std::chrono::steady_clock>{
        std::chrono::seconds{}})
  } -> std::same_as<bool>;
};

template <typename T>
concept basic_shared_lockable = basic_lockable<T> && requires(T a) {
  { a.lock_shared() } -> std::same_as<void>;
  { a.unlock_shared() } -> std::same_as<void>;
};

template <typename T>
concept shared_lockable = basic_shared_lockable<T> && requires(T a) {
  { a.try_lock_shared() } -> std::same_as<bool>;
};

template <typename T>
concept shared_timed_lockable = shared_lockable<T> && requires(T a) {
  { a.try_lock_shared_for(std::chrono::seconds{}) } -> std::same_as<bool>;

  {
    a.try_lock_shared_until(std::chrono::time_point<std::chrono::steady_clock>{
        std::chrono::seconds{}})
  } -> std::same_as<bool>;
};

template <typename T, typename Lockable, template <typename...> typename Lock>
  requires(std::is_reference_v<T>)
class accessor : private noncopyable {
 public:
  constexpr explicit accessor(Lockable& lockable, T resource)
    requires(!std::is_const_v<Lockable>)
      : lock(lockable), locked_resource(resource) {}

  // Constructor for moved lock (used in timeout methods)
  constexpr explicit accessor(Lock<Lockable>&& moved_lock, T resource)
    requires(!std::is_const_v<Lockable>)
      : lock(std::move(moved_lock)), locked_resource(resource) {}

  // Optimized: directly return pointer to reduce indirection
  inline auto* operator->() noexcept { return &locked_resource; }

  inline decltype(auto) operator*() noexcept { return locked_resource; }

 private:
  Lock<Lockable> lock;
  T locked_resource;
};

template <typename T, basic_shared_lockable Lockable,
          template <typename...> typename Lock>
using shared_accessor =
    accessor<std::add_lvalue_reference_t<std::add_const_t<T>>, Lockable, Lock>;

template <typename T, basic_lockable Lockable,
          template <typename...> typename Lock>
using exclusive_accessor =
    accessor<std::add_lvalue_reference_t<T>, Lockable, Lock>;

template <typename T, basic_shared_lockable Lockable,
          template <typename...> typename SharedLock,
          template <typename...> typename ExclusiveLock>
class concurrent {
 public:
  using shared_accessor_t = shared_accessor<T, Lockable, SharedLock>;
  using exclusive_accessor_t = exclusive_accessor<T, Lockable, ExclusiveLock>;

  // Statistics structure for performance analysis
  // Internal atomic counters for thread-safe updates
  struct StatisticsData {
    std::atomic<size_t> read_count{0};
    std::atomic<size_t> write_count{0};
    std::atomic<size_t> read_wait_time_ns{0};
    std::atomic<size_t> write_wait_time_ns{0};
  };

  // Snapshot structure returned to users (non-atomic for easy access)
  struct Statistics {
    size_t read_count{0};
    size_t write_count{0};
    size_t read_wait_time_ns{0};
    size_t write_wait_time_ns{0};
  };

  concurrent() noexcept(std::is_nothrow_default_constructible_v<T>)
    requires(std::is_default_constructible_v<T>)
      : resource{} {}

  explicit concurrent(const T& value) noexcept(
      std::is_nothrow_copy_constructible_v<T>)
    requires(std::is_copy_constructible_v<T>)
      : resource(value) {}

  explicit concurrent(T&& value) noexcept(
      std::is_nothrow_move_constructible_v<T>)
    requires(std::is_move_constructible_v<T>)
      : resource(std::move(value)) {}

  ~concurrent() noexcept(std::is_nothrow_destructible_v<T>) = default;

  template <typename Concurrent>
  static decltype(auto) make_read_accessor(Concurrent&& con) noexcept {
    auto start = std::chrono::steady_clock::now();
    auto accessor = shared_accessor_t{con.lockable, con.resource};
    auto end = std::chrono::steady_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         end - start)
                         .count();
    con.stats.read_count.fetch_add(1, std::memory_order_relaxed);
    con.stats.read_wait_time_ns.fetch_add(
        static_cast<size_t>(wait_time), std::memory_order_relaxed);
    return accessor;
  }

  template <typename Concurrent>
  static decltype(auto) make_write_accessor(Concurrent&& con) noexcept {
    auto start = std::chrono::steady_clock::now();
    auto accessor = exclusive_accessor_t{con.lockable, con.resource};
    auto end = std::chrono::steady_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         end - start)
                         .count();
    con.stats.write_count.fetch_add(1, std::memory_order_relaxed);
    con.stats.write_wait_time_ns.fetch_add(
        static_cast<size_t>(wait_time), std::memory_order_relaxed);
    return accessor;
  }

  // Timeout support for read accessor
  template <typename Concurrent, typename Rep, typename Period>
  static std::optional<shared_accessor_t> try_make_read_accessor(
      Concurrent&& con,
      const std::chrono::duration<Rep, Period>& timeout) noexcept
    requires(shared_timed_lockable<Lockable>) {
    auto start = std::chrono::steady_clock::now();
    SharedLock<Lockable> lock(con.lockable, timeout);
    if (!lock.owns_lock()) {
      return std::nullopt;
    }
    auto end = std::chrono::steady_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         end - start)
                         .count();
    con.stats.read_count.fetch_add(1, std::memory_order_relaxed);
    con.stats.read_wait_time_ns.fetch_add(
        static_cast<size_t>(wait_time), std::memory_order_relaxed);
    return shared_accessor_t{std::move(lock), con.resource};
  }

  // Timeout support for write accessor
  template <typename Concurrent, typename Rep, typename Period>
  static std::optional<exclusive_accessor_t> try_make_write_accessor(
      Concurrent&& con,
      const std::chrono::duration<Rep, Period>& timeout) noexcept
    requires(timed_lockable<Lockable>) {
    auto start = std::chrono::steady_clock::now();
    ExclusiveLock<Lockable> lock(con.lockable, timeout);
    if (!lock.owns_lock()) {
      return std::nullopt;
    }
    auto end = std::chrono::steady_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         end - start)
                         .count();
    con.stats.write_count.fetch_add(1, std::memory_order_relaxed);
    con.stats.write_wait_time_ns.fetch_add(
        static_cast<size_t>(wait_time), std::memory_order_relaxed);
    return exclusive_accessor_t{std::move(lock), con.resource};
  }

  // Get statistics (returns a copy for thread-safety)
  Statistics get_stats() const noexcept {
    Statistics result;
    result.read_count = stats.read_count.load(std::memory_order_relaxed);
    result.write_count = stats.write_count.load(std::memory_order_relaxed);
    result.read_wait_time_ns =
        stats.read_wait_time_ns.load(std::memory_order_relaxed);
    result.write_wait_time_ns =
        stats.write_wait_time_ns.load(std::memory_order_relaxed);
    return result;
  }

 private:
  T resource;
  mutable Lockable lockable;
  mutable StatisticsData stats;
};

template <typename UnderlyingMap>
class ConcurrentMap : public concurrent<UnderlyingMap, std::shared_mutex, std::shared_lock, std::unique_lock> {
 public:
  using underlying_type = UnderlyingMap;
  using key_type = typename UnderlyingMap::key_type;
  using value_type = typename UnderlyingMap::value_type;
  using iterator = typename UnderlyingMap::iterator;
  using const_iterator = typename UnderlyingMap::const_iterator;
};

template <typename UnderlyingVector>
class ConcurrentVector : public concurrent<UnderlyingVector, std::shared_mutex, std::shared_lock, std::unique_lock> {
 public:
  using underlying_type = UnderlyingVector;
  using value_type = typename UnderlyingVector::value_type;
  using iterator = typename UnderlyingVector::iterator;
  using const_iterator = typename UnderlyingVector::const_iterator;
};
}  // namespace base

#endif  // BASE_CONCURRENCY_CONCURRENT_H
