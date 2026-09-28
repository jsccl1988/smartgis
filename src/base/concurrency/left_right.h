// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONCURRENCY_LEFT_RIGHT_H
#define BASE_CONCURRENCY_LEFT_RIGHT_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace base {
// Left-Right concurrency pattern:
//   - Writers never block readers: read operations are wait-free
//     (population oblivious)
//   - Readers never block writers: updated data is immediately
//     visible to new readers
//   - Optimized memory ordering and exponential backoff for better performance
//
// Usage:
//   base::left_right<std::map<int, int>> lr;
//   lr.read([](const auto& map) { /* read-only access */ });
//   lr.update([](auto& map) { /* modify map */ });
template <typename T>
struct left_right {
  explicit left_right(T source) : _left(source), _right(std::move(source)) {}
  left_right(T left, T right)
      : _left(std::move(left)), _right(std::move(right)) {}

  // std::mutex / atomics are not movable; default move is ill-formed. Construct fresh
  // synchronization state and move only the two T sides (caller must ensure no races).
  left_right(left_right&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
      : _writer_mutex(),
        _version_index(0),
        _lr_indicator(READ_LEFT),
        _read_indicator1(),
        _left(std::move(other._left)),
        _read_indicator2(),
        _right(std::move(other._right)) {}

  left_right() = default;

  // Initialize or reset the data (for cases where assignment is not possible)
  void init(T source) {
    _left = source;
    _right = std::move(source);
    _version_index.store(0, std::memory_order_release);
    _lr_indicator.store(READ_LEFT, std::memory_order_release);
    // Note: read_indicator members are already default-initialized and cannot be reassigned
    // as they contain std::atomic which is not copyable/movable
  }

  // Read operation: wait-free, never blocks writers
  template <typename Fn>
  auto read(Fn&& fn) const {
    read_guard guard(*this);
    const int indicator = _lr_indicator.load(std::memory_order_acquire);
    return fn(indicator == READ_LEFT ? _left : _right);
  }

  // Update operation: blocks other writers, but not readers
  template <typename Fn>
  void update(Fn&& fn) {
    std::lock_guard<std::mutex> lock(_writer_mutex);
    const int indicator = _lr_indicator.load(std::memory_order_acquire);

    if (indicator == READ_LEFT) {
      fn(_right);
      _lr_indicator.store(READ_RIGHT, std::memory_order_release);
      toggle_version_and_wait();
      fn(_left);
    } else {
      fn(_left);
      _lr_indicator.store(READ_LEFT, std::memory_order_release);
      toggle_version_and_wait();
      fn(_right);
    }
  }

 private:
  // Per-version read indicator with cache line alignment to avoid false sharing
  struct alignas(64) read_indicator {
    void arrive() {
      _counter.fetch_add(1, std::memory_order_acq_rel);
    }
    void depart() {
      _counter.fetch_sub(1, std::memory_order_release);
    }
    bool empty() const {
      return _counter.load(std::memory_order_acquire) == 0;
    }

   private:
    std::atomic<uint64_t> _counter{0};
  };

  struct read_guard {
    explicit read_guard(const left_right& inst)
        : _indicator(inst.get_read_indicator(
              inst._version_index.load(std::memory_order_acquire))) {
      _indicator.arrive();
    }
    ~read_guard() { _indicator.depart(); }
    read_guard(const read_guard&) = delete;
    read_guard& operator=(const read_guard&) = delete;

   private:
    read_indicator& _indicator;
  };
  friend struct read_guard;

  void toggle_version_and_wait() {
    const int current_idx = _version_index.load(std::memory_order_acquire) & 0x1;
    const int next_idx = 1 - current_idx;

    wait_for_readers(next_idx);
    _version_index.store(next_idx, std::memory_order_release);
    wait_for_readers(current_idx);
  }

  void wait_for_readers(int idx) {
    auto& indicator = get_read_indicator(idx);
    int spin_count = 0;
    constexpr int MAX_SPIN = 100;
    constexpr int MAX_YIELD = 1000;
    constexpr int MAX_SLEEP_SHIFT = 20;  // Limit sleep duration to prevent overflow

    while (!indicator.empty()) {
      if (spin_count < MAX_SPIN) {
        ++spin_count;
        std::atomic_signal_fence(std::memory_order_acq_rel);
      } else if (spin_count < MAX_YIELD) {
        std::this_thread::yield();
        ++spin_count;
      } else {
        int sleep_shift = std::min(spin_count - MAX_YIELD, MAX_SLEEP_SHIFT);
        std::this_thread::sleep_for(
            std::chrono::microseconds(1 << sleep_shift));
        ++spin_count;
      }
    }
  }

  read_indicator& get_read_indicator(int idx) const {
    assert(idx == 0 || idx == 1);
    return (idx == 0) ? _read_indicator1 : _read_indicator2;
  }

  static constexpr int READ_LEFT = 0;
  static constexpr int READ_RIGHT = 1;

  alignas(64) std::mutex _writer_mutex;
  alignas(64) std::atomic<int> _version_index{0};
  alignas(64) std::atomic<int> _lr_indicator{READ_LEFT};
  alignas(64) mutable read_indicator _read_indicator1;
  alignas(64) T _left;
  alignas(64) mutable read_indicator _read_indicator2;
  alignas(64) T _right;
};
}  // namespace base

#endif  // BASE_CONCURRENCY_LEFT_RIGHT_H