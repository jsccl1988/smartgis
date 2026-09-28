// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Lock-free per-slot ownership for partitioned writers. Distinct partition
// acquires do not share a global spin_lock / mutex.

#ifndef BASE_CONCURRENCY_PARTITION_H
#define BASE_CONCURRENCY_PARTITION_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace base {

// Tracks which partition slots are held. Intended for debug / tests / optional
// runtime checks. resize() is setup-only — not concurrent with acquire/release.
class partition_exclusivity {
 public:
  explicit partition_exclusivity(std::size_t partition_count = 0) {
    resize(partition_count);
  }

  void resize(std::size_t partition_count) {
    if (partition_count == 0) {
      held_.reset();
      size_ = 0;
      return;
    }
    auto next = std::make_unique<std::atomic<std::uint8_t>[]>(partition_count);
    for (std::size_t i = 0; i < partition_count; ++i) {
      next[i].store(0, std::memory_order_relaxed);
    }
    held_ = std::move(next);
    size_ = partition_count;
  }

  std::size_t size() const { return size_; }

  // Acquire exclusive write for partition. Returns false if already held.
  bool try_acquire(std::size_t partition) {
    if (partition >= size_) {
      return false;
    }
    std::uint8_t expected = 0;
    return held_[partition].compare_exchange_strong(
        expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

  void release(std::size_t partition) {
    if (partition >= size_) {
      return;
    }
    held_[partition].store(0, std::memory_order_release);
  }

  bool is_held(std::size_t partition) const {
    if (partition >= size_) {
      return false;
    }
    return held_[partition].load(std::memory_order_acquire) != 0;
  }

 private:
  // 0 = free, 1 = held. Dense uint8_t; false sharing OK for debug traffic.
  std::unique_ptr<std::atomic<std::uint8_t>[]> held_;
  std::size_t size_{0};
};

// RAII acquire for one partition. In debug builds, asserts exclusivity.
class scoped_partition_write {
 public:
  scoped_partition_write(partition_exclusivity& exclusivity,
                         std::size_t partition)
      : exclusivity_(&exclusivity),
        partition_(partition),
        held_(exclusivity.try_acquire(partition)) {
#ifndef NDEBUG
    assert(held_ &&
           "base: partition exclusivity violated (one writer per partition)");
#endif
  }

  ~scoped_partition_write() {
    if (held_ && exclusivity_) {
      exclusivity_->release(partition_);
    }
  }

  scoped_partition_write(const scoped_partition_write&) = delete;
  scoped_partition_write& operator=(const scoped_partition_write&) = delete;

  bool held() const { return held_; }
  std::size_t partition() const { return partition_; }

 private:
  partition_exclusivity* exclusivity_{nullptr};
  std::size_t partition_{0};
  bool held_{false};
};

}  // namespace base

#endif  // BASE_CONCURRENCY_PARTITION_H
