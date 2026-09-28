// Copyright (c) 2023 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONCURRENCY_HAZARD_PTR_H
#define BASE_CONCURRENCY_HAZARD_PTR_H

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

namespace base {
// Hazard Pointer implementation for safe memory reclamation in lock-free data
// structures.
//
// Usage:
//   std::atomic<Node*> head;
//   auto holder = base::hazard_ptr<Node>::acquire(head);
//   if (holder) {
//     // Safe to access *holder
//   }
//   base::hazard_ptr<Node>::update(head, new_node);
//   base::hazard_ptr<Node>::reclaim();  // Call periodically or on threshold
//
// Custom deleter:
//   base::hazard_ptr<Node>::update(head, new_node, [](Node* p) { delete p; });
template <class T>
struct hazard_ptr {
 public:
  // Default deleter: delete pointer
  using deleter_t = std::function<void(T*)>;
  static deleter_t default_deleter() {
    return [](T* p) { delete p; };
  }

  class holder {
   public:
    explicit holder(hazard_ptr<T>* ptr) : ptr_(ptr) {}
    holder(const holder&) = delete;
    holder& operator=(const holder&) = delete;
    holder(holder&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    holder& operator=(holder&& other) noexcept {
      if (this != &other) {
        if (ptr_) {
          ptr_->release();
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
      }
      return *this;
    }
    ~holder() {
      if (ptr_) {
        ptr_->release();
      }
    }
    T* get() const {
      return ptr_ ? ptr_->target_.load(std::memory_order_acquire) : nullptr;
    }
    explicit operator bool() const { return get() != nullptr; }
    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }

   private:
    hazard_ptr<T>* ptr_;
  };

 public:
  ~hazard_ptr() = default;

  void release() {
    target_.store(nullptr, std::memory_order_release);
    active_.clear(std::memory_order_release);
  }

  // Acquire a hazard pointer for the given atomic target
  // Returns a holder that protects the pointer until destruction
  static holder acquire(const std::atomic<T*>& target) {
    auto ptr = alloc();
    if (!ptr) {
      return holder(nullptr);
    }

    // Double-check pattern: ensure we get a consistent snapshot
    T* snapshot;
    do {
      snapshot = target.load(std::memory_order_acquire);
      ptr->target_.store(snapshot, std::memory_order_release);
    } while (target.load(std::memory_order_acquire) != snapshot);

    return holder(ptr);
  }

  static void update(std::atomic<T*>& target, T* new_target) {
    update(target, new_target, default_deleter());
  }

  static void update(std::atomic<T*>& target, T* new_target,
                     deleter_t deleter) {
    T* old = target.exchange(new_target, std::memory_order_acq_rel);
    if (old != nullptr) {
      retire(old, deleter);
    }
  }

  // Retire a batch of pointers with one allocation. Use in clear() to avoid
  // N allocations (each retire() does "new list"). Call reclaim() after.
  static void retire_batch(const std::vector<T*>& ptrs, deleter_t deleter) {
    if (ptrs.empty()) return;
    auto* node = new batch_list;
    node->ptrs = ptrs;
    node->deleter = deleter;
    batch_list* expected = retire_batch_list_.load(std::memory_order_acquire);
    do {
      node->next = expected;
    } while (!retire_batch_list_.compare_exchange_weak(
        expected, node, std::memory_order_release, std::memory_order_acquire));
  }

  static void reclaim() {
    std::unordered_set<T*> in_use;
    for (auto p = head_list_.load(std::memory_order_acquire); p;
         p = p->next_) {
      T* target = p->target_.load(std::memory_order_acquire);
      if (target != nullptr) {
        in_use.insert(target);
      }
    }

    list* retire_head = nullptr;
    list* retire_tail = nullptr;

    auto p = retire_list_.exchange(nullptr, std::memory_order_acq_rel);
    while (p != nullptr) {
      auto next = p->next;
      T* target = p->target;

      if (target != nullptr && in_use.count(target) == 0) {
        (p->deleter ? p->deleter : default_deleter())(target);
        delete p;
      } else {
        p->next = retire_head;
        retire_head = p;
        if (retire_tail == nullptr) {
          retire_tail = p;
        }
      }

      p = next;
    }

    if (retire_head != nullptr) {
      list* expected = retire_list_.load(std::memory_order_acquire);
      do {
        retire_tail->next = expected;
      } while (!retire_list_.compare_exchange_weak(
          expected, retire_head, std::memory_order_release,
          std::memory_order_acquire));
    }

    // Process batch retires (one allocation per batch, avoids N allocations in clear())
    auto* batch = retire_batch_list_.exchange(nullptr, std::memory_order_acq_rel);
    while (batch != nullptr) {
      batch_list* next = batch->next;
      for (T* target : batch->ptrs) {
        if (target != nullptr && in_use.count(target) == 0) {
          (batch->deleter ? batch->deleter : default_deleter())(target);
        }
      }
      delete batch;
      batch = next;
    }
  }

 private:
  static hazard_ptr<T>* alloc() {
    for (auto p = head_list_.load(std::memory_order_acquire); p;
         p = p->next_) {
      if (!p->active_.test_and_set(std::memory_order_acq_rel)) {
        return p;
      }
    }

    auto p = new hazard_ptr<T>();
    p->active_.test_and_set(std::memory_order_acq_rel);

    hazard_ptr<T>* expected = head_list_.load(std::memory_order_acquire);
    do {
      p->next_ = expected;
    } while (!head_list_.compare_exchange_weak(
        expected, p, std::memory_order_release, std::memory_order_acquire));

    return p;
  }

  static void retire(T* ptr, deleter_t deleter = default_deleter()) {
    if (ptr == nullptr) {
      return;
    }

    auto p = new list;
    p->target = ptr;
    p->deleter = deleter;

    list* expected = retire_list_.load(std::memory_order_acquire);
    do {
      p->next = expected;
    } while (!retire_list_.compare_exchange_weak(
        expected, p, std::memory_order_release, std::memory_order_acquire));

    uint32_t count = retire_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (count >= RECLAIM_THRESHOLD) {
      retire_count_.store(0, std::memory_order_relaxed);
      reclaim();
    }
  }

 private:
  struct list {
    T* target{nullptr};
    deleter_t deleter;
    list* next = nullptr;
  };

  struct batch_list {
    std::vector<T*> ptrs;
    deleter_t deleter;
    batch_list* next = nullptr;
  };

 private:
  static constexpr uint32_t RECLAIM_THRESHOLD = 1000;

  std::atomic<T*> target_{nullptr};
  hazard_ptr<T>* next_{nullptr};
  std::atomic_flag active_{};

  static std::atomic<hazard_ptr<T>*> head_list_;
  static std::atomic<uint32_t> retire_count_;
  static std::atomic<list*> retire_list_;
  static std::atomic<batch_list*> retire_batch_list_;
};

template <class T>
std::atomic<hazard_ptr<T>*> hazard_ptr<T>::head_list_{nullptr};
template <class T>
std::atomic<uint32_t> hazard_ptr<T>::retire_count_{0};
template <class T>
std::atomic<typename hazard_ptr<T>::list*> hazard_ptr<T>::retire_list_{
    nullptr};
template <class T>
std::atomic<typename hazard_ptr<T>::batch_list*>
    hazard_ptr<T>::retire_batch_list_{nullptr};
}  // namespace base
#endif  // BASE_CONCURRENCY_HAZARD_PTR_H