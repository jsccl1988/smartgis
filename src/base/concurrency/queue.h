// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONCURRENCY_QUEUE_H
#define BASE_CONCURRENCY_QUEUE_H

#include <utility>
#include <vector>

#include "base/core/log.h"
#include "base/synchronization/align.h"
#include "concurrentqueue/moodycamel/blockingconcurrentqueue.h"
#include "concurrentqueue/moodycamel/concurrentqueue.h"

// When producing or consuming many elements, the most efficient way is to:
// 1.Use the bulk methods of the queue with tokens
// 2.Failing that, use the bulk methods without tokens
// 3.Failing that, use the single-item methods with tokens
// 4.Failing that, use the single-item methods without tokens
namespace base {
template <typename T>
class NonblockingQueue {
 public:
  using value_type = T;
  template <typename U>
  bool push(U &&obj) {
    return _queue.enqueue(std::forward<U>(obj));
  }
  template <typename Array>
  bool push_bulk(Array arr, std::size_t count) {
    return _queue.enqueue_bulk(arr, count);
  }

  bool pop(T &obj) { return _queue.try_dequeue(obj); }
  template <typename Array>
  std::size_t pop_bulk(Array arr, std::size_t count) {
    return _queue.try_dequeue_bulk(arr, count);
  }

  std::size_t size_approx() const { return _queue.size_approx(); }

 private:
  moodycamel::ConcurrentQueue<T> _queue;
};

// wait_pop* spin-cpu to block current thread, will cost more cpu-idle, so we
// can't use as thread pool's task queue
template <typename T>
class BlockingQueue {
 public:
  using value_type = T;
  std::size_t size_approx() const { return _queue.size_approx(); }

  bool push(const T &obj) { return _queue.enqueue(obj); }
  template <typename U>
  bool push(U &&obj) {
    return _queue.enqueue(std::forward<U>(obj));
  }
  template <typename Array>
  bool push_bulk(Array arr, std::size_t count) {
    return _queue.enqueue_bulk(arr, count);
  }

  bool pop(T &obj) { return _queue.try_dequeue(obj); }
  template <typename Array>
  std::size_t pop_bulk(Array arr, std::size_t count) {
    return _queue.try_dequeue_bulk(arr, count);
  }

  void wait_pop(T &obj) { _queue.wait_dequeue(obj); }
  bool wait_pop_timed(T &obj, std::int64_t timeout_usecs) {
    return _queue.wait_dequeue_timed(obj, timeout_usecs);
  }
  template <typename Array>
  std::size_t wait_pop_bulk(Array arr, std::size_t count) {
    return _queue.wait_dequeue_bulk(arr, count);
  }
  template <typename Array>
  std::size_t wait_pop_bulk_timed(Array arr, std::size_t count,
                                  std::int64_t timeout_usecs) {
    return _queue.wait_dequeue_bulk_timed(arr, count, timeout_usecs);
  }

 private:
  moodycamel::BlockingConcurrentQueue<T> _queue;
};

template <class T>
using ConcurrentQueue = base::NonblockingQueue<T>;

// Increment-only MPMC queue: producers enqueue*; one drain uses gc(fn) /
// gc_discard(). Prefer concurrent_append_buffer for typed drain helpers.
// Distinct from PushOnlyQueue (visit then clear) — see concurrency/README.
template <typename T>
class AppendOnlyConcurrentQueue {
 public:
  using producer_token_t =
      typename moodycamel::ConcurrentQueue<T>::producer_token_t;
  using consumer_token_t =
      typename moodycamel::ConcurrentQueue<T>::consumer_token_t;

  explicit AppendOnlyConcurrentQueue(
      size_t capacity = 32 * moodycamel::ConcurrentQueue<T>::BLOCK_SIZE)
      : queue_(capacity) {}

  AppendOnlyConcurrentQueue(size_t min_capacity, size_t max_explicit_producers,
                            size_t max_implicit_producers)
      : queue_(min_capacity, max_explicit_producers, max_implicit_producers) {}

  AppendOnlyConcurrentQueue(const AppendOnlyConcurrentQueue&) = delete;
  AppendOnlyConcurrentQueue& operator=(const AppendOnlyConcurrentQueue&) =
      delete;
  AppendOnlyConcurrentQueue(AppendOnlyConcurrentQueue&&) = default;
  AppendOnlyConcurrentQueue& operator=(AppendOnlyConcurrentQueue&&) = default;

  bool enqueue(const T& item) { return queue_.enqueue(item); }
  bool enqueue(T&& item) { return queue_.enqueue(std::move(item)); }

  bool enqueue(const producer_token_t& token, const T& item) {
    return queue_.enqueue(token, item);
  }
  bool enqueue(const producer_token_t& token, T&& item) {
    return queue_.enqueue(token, std::move(item));
  }

  template <typename It>
  bool enqueue_bulk(It item_first, size_t count) {
    return queue_.enqueue_bulk(std::forward<It>(item_first), count);
  }

  template <typename It>
  bool enqueue_bulk(const producer_token_t& token, It item_first,
                    size_t count) {
    return queue_.enqueue_bulk(token, std::forward<It>(item_first), count);
  }

  // Drain visible elements; not concurrent with another gc — serialize.
  template <typename Fn>
  size_t gc(Fn&& fn) {
    size_t drained = 0;
    T item;
    while (queue_.try_dequeue(item)) {
      std::forward<Fn>(fn)(std::move(item));
      ++drained;
    }
    return drained;
  }

  size_t gc_discard() {
    size_t drained = 0;
    T item;
    while (queue_.try_dequeue(item)) {
      ++drained;
    }
    return drained;
  }

  [[nodiscard]] size_t size_approx() const { return queue_.size_approx(); }

  [[nodiscard]] moodycamel::ConcurrentQueue<T>& underlying() noexcept {
    return queue_;
  }
  [[nodiscard]] const moodycamel::ConcurrentQueue<T>& underlying() const
      noexcept {
    return queue_;
  }

 private:
  moodycamel::ConcurrentQueue<T> queue_;
};

template <typename T>
class PushOnlyQueue {
 public:
  ~PushOnlyQueue() { clear(); }

  void push(const T& val) {
    _queue.push(val);
  }

  bool empty() const {
    return _queue.size_approx() == 0;
  }

  void clear() {
    T obj;
    while (_queue.pop(obj)) {
    }
  }

  template <class Visitor>
  void visit(Visitor& visitor) {
    std::vector<T> temp;
    T obj;
    while (_queue.pop(obj)) {
      temp.push_back(std::move(obj));
    }

    for (auto& val : temp) {
      visitor(val);
    }
  }

 private:
  NonblockingQueue<T> _queue;
};
}  // namespace base
#endif  // BASE_CONCURRENCY_QUEUE_H
