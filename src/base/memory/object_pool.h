// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_OBJECT_POOL_H
#define BASE_MEMORY_OBJECT_POOL_H

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "base/memory/noncopyable.h"
#include "base/memory/memory_resource.h"

namespace base {

// Thread-safe free-list of T with optional MemoryResource backing, placement
// construction, factory allocation, and reset-on-reuse. allocate() returns
// shared_ptr with custom deleter that returns instances to the pool.
template <typename T>
class ObjectPool : private noncopyable {
 public:
  using value_type = std::shared_ptr<T>;
  using Factory = std::function<std::unique_ptr<T>()>;
  /// In-place construction at pool-allocated storage: `[](T* p) { new (p) T(...); }`
  using PlacementFactory = std::function<void(T*)>;
  using Reset = std::function<void(T*)>;

  ObjectPool(size_t max_size = 0, Factory factory = nullptr, Reset reset = nullptr,
             MemoryResource* memory_resource = nullptr,
             PlacementFactory placement_factory = nullptr)
      : max_size_(max_size),
        factory_(std::move(factory)),
        reset_(std::move(reset)),
        memory_resource_(memory_resource),
        placement_factory_(std::move(placement_factory)) {}

  ~ObjectPool() {
    for (T* obj : objects_) {
      destroy_object(obj);
    }
    objects_.clear();
  }

  value_type allocate() {
    T* obj = nullptr;
    bool from_pool = false;

    if (!objects_.empty()) {
      obj = objects_.back();
      objects_.pop_back();
      total_recycled_.fetch_add(1, std::memory_order_relaxed);
      from_pool = true;
    } else {
      obj = create_fresh();
      total_created_.fetch_add(1, std::memory_order_relaxed);
      note_peak_pool_size();
    }

    if (from_pool && reset_) {
      reset_(obj);
    }

    outstanding_.fetch_add(1, std::memory_order_relaxed);
    note_peak_outstanding();
    return value_type(obj, [this](T* t) { recycle(t); });
  }

  /// `pool_size` = idle objects in the free list. `outstanding` = objects held
  /// by clients. `peak_outstanding` = max concurrent outstanding.
  struct Statistics {
    size_t pool_size{0};
    size_t total_created{0};
    size_t total_recycled{0};
    size_t peak_size{0};
    size_t outstanding{0};
    size_t peak_outstanding{0};
  };

  Statistics get_statistics() const {
    return {
        objects_.size(),
        total_created_.load(std::memory_order_relaxed),
        total_recycled_.load(std::memory_order_relaxed),
        peak_size_.load(std::memory_order_relaxed),
        outstanding_.load(std::memory_order_relaxed),
        peak_outstanding_.load(std::memory_order_relaxed),
    };
  }

  void set_max_size(size_t max_size) {
    max_size_ = max_size;
    trim_pool();
  }

  size_t max_size() const { return max_size_; }

  /// Pre-create up to `n` idle objects. When `max_size > 0`, caps at `max_size`.
  void reserve(size_t n) {
    if (n == 0) {
      return;
    }
    const size_t cap = (max_size_ == 0 || n < max_size_) ? n : max_size_;
    while (objects_.size() < cap) {
      T* obj = create_fresh();
      objects_.push_back(obj);
      total_created_.fetch_add(1, std::memory_order_relaxed);
      note_peak_pool_size();
    }
  }

  MemoryResource* get_memory_resource() const { return memory_resource_; }

 private:
  void* allocate_block() {
    if (memory_resource_) {
      return memory_resource_->allocate(sizeof(T), alignof(T));
    }
    return ::operator new(sizeof(T));
  }

  void deallocate_block(void* ptr) {
    if (memory_resource_) {
      memory_resource_->deallocate(ptr, sizeof(T), alignof(T));
    } else {
      ::operator delete(ptr);
    }
  }

  /// One allocation + construction policy matrix (see class comment).
  T* create_fresh() {
    if (factory_ && !memory_resource_) {
      return factory_().release();
    }

    void* p = allocate_block();
    try {
      if (placement_factory_) {
        T* obj = static_cast<T*>(p);
        placement_factory_(obj);
        return obj;
      }
      if (factory_ && memory_resource_) {
        std::unique_ptr<T> u = factory_();
        T* temp = u.release();
        T* obj = new (p) T(std::move(*temp));
        delete temp;
        return obj;
      }
      if (factory_) {
        deallocate_block(p);
        return factory_().release();
      }
      return new (p) T();
    } catch (...) {
      deallocate_block(p);
      throw;
    }
  }

  void destroy_object(T* obj) {
    if (!obj) {
      return;
    }
    obj->~T();
    deallocate_block(obj);
  }

  void recycle(T* obj) {
    outstanding_.fetch_sub(1, std::memory_order_relaxed);
    if (max_size_ > 0 && objects_.size() >= max_size_) {
      destroy_object(obj);
      return;
    }
    objects_.push_back(obj);
  }

  void note_peak_pool_size() {
    const size_t current = objects_.size();
    size_t peak = peak_size_.load(std::memory_order_relaxed);
    if (current > peak) {
      peak_size_.store(current, std::memory_order_relaxed);
    }
  }

  void trim_pool() {
    if (max_size_ == 0) {
      return;
    }
    while (objects_.size() > max_size_) {
      destroy_object(objects_.back());
      objects_.pop_back();
    }
  }

  void note_peak_outstanding() {
    const size_t o = outstanding_.load(std::memory_order_relaxed);
    size_t p = peak_outstanding_.load(std::memory_order_relaxed);
    while (o > p) {
      if (peak_outstanding_.compare_exchange_weak(
              p, o, std::memory_order_relaxed, std::memory_order_relaxed)) {
        return;
      }
    }
  }

  std::vector<T*> objects_;
  size_t max_size_{0};
  Factory factory_;
  Reset reset_;
  MemoryResource* memory_resource_{nullptr};
  PlacementFactory placement_factory_;
  std::atomic<size_t> total_created_{0};
  std::atomic<size_t> total_recycled_{0};
  std::atomic<size_t> peak_size_{0};
  std::atomic<size_t> outstanding_{0};
  std::atomic<size_t> peak_outstanding_{0};
};

}  // namespace base

#endif  // BASE_MEMORY_OBJECT_POOL_H
