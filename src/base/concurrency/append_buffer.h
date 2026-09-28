// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Multi-producer append buffer over AppendOnlyConcurrentQueue. Prefer
// partition_exclusivity + direct slot write when exclusivity holds; use this
// when several writers must fan-in updates for one logical item.
// Distinct from PushOnlyQueue (visit/clear semantics) — see concurrency/README.

#ifndef BASE_CONCURRENCY_APPEND_BUFFER_H
#define BASE_CONCURRENCY_APPEND_BUFFER_H

#include <cstddef>
#include <utility>

#include "base/concurrency/queue.h"

namespace base {

template <class T>
class concurrent_append_buffer {
 public:
  using value_type = T;
  using producer_token_t =
      typename AppendOnlyConcurrentQueue<T>::producer_token_t;

  concurrent_append_buffer() = default;
  explicit concurrent_append_buffer(std::size_t capacity) : queue_(capacity) {}

  bool push(const T& item) { return queue_.enqueue(item); }
  bool push(T&& item) { return queue_.enqueue(std::move(item)); }

  bool push(const producer_token_t& token, const T& item) {
    return queue_.enqueue(token, item);
  }
  bool push(const producer_token_t& token, T&& item) {
    return queue_.enqueue(token, std::move(item));
  }

  // Drain visible elements; fn receives each by move. Not concurrent with
  // another drain — same contract as AppendOnlyConcurrentQueue::gc.
  template <typename Fn>
  std::size_t drain(Fn&& fn) {
    return queue_.gc(std::forward<Fn>(fn));
  }

  std::size_t discard() { return queue_.gc_discard(); }

  [[nodiscard]] std::size_t size_approx() const {
    return queue_.size_approx();
  }

  [[nodiscard]] AppendOnlyConcurrentQueue<T>& underlying() noexcept {
    return queue_;
  }
  [[nodiscard]] const AppendOnlyConcurrentQueue<T>& underlying() const
      noexcept {
    return queue_;
  }

 private:
  AppendOnlyConcurrentQueue<T> queue_;
};

}  // namespace base

#endif  // BASE_CONCURRENCY_APPEND_BUFFER_H
